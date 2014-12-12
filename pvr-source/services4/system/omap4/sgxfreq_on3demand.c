/*
 * Copyright (C) 2012 Texas Instruments, Inc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include "sgxfreq.h"

static int on3demand_start(struct sgxfreq_sgx_data *data);
static void on3demand_stop(void);
static void on3demand_predict(void);
static void on3demand_frame_done(void);
static void on3demand_active(void);
static void on3demand_timeout(struct work_struct *work);


static struct sgxfreq_governor on3demand_gov = {
	.name =	"on3demand",
	.gov_start = on3demand_start,
	.gov_stop = on3demand_stop,
	.sgx_frame_done = on3demand_frame_done,
	.sgx_active = on3demand_active,
};

static struct on3demand_data {
	unsigned int load;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int history_size;
	unsigned long prev_total_idle;
	unsigned long prev_total_active;
	unsigned int low_load_cnt;
	unsigned int poll_interval;
	unsigned long delta_active;
	unsigned long delta_idle;
	bool polling_enabled;
	struct delayed_work work;
	struct mutex mutex;
} odd;

#define ON3DEMAND_DEFAULT_UP_THRESHOLD			80
#define ON3DEMAND_DEFAULT_DOWN_THRESHOLD		30
#define ON3DEMAND_DEFAULT_HISTORY_SIZE_THRESHOLD	5
/* For Live wallpaper frame done at interval of ~64ms */
#define ON3DEMAND_DEFAULT_POLL_INTERVAL			75

/*FIXME: This should be dynamic and queried from platform */
#define ON3DEMAND_FRAME_DONE_DEADLINE_MS 16


/*********************** begin sysfs interface ***********************/

extern struct kobject *sgxfreq_kobj;

static ssize_t show_down_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", odd.down_threshold);
}

static ssize_t store_down_threshold(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int thres;

	ret = sscanf(buf, "%u", &thres);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&odd.mutex);

	if (thres <= 100) {
		odd.down_threshold = thres;
		odd.low_load_cnt = 0;
	} else {
		return -EINVAL;
	}

	mutex_unlock(&odd.mutex);

	return count;
}

static ssize_t show_up_threshold(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", odd.up_threshold);
}

static ssize_t store_up_threshold(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int thres;

	ret = sscanf(buf, "%u", &thres);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&odd.mutex);

	if (thres <= 100) {
		odd.up_threshold = thres;
		odd.low_load_cnt = 0;
	} else {
		return -EINVAL;
	}

	mutex_unlock(&odd.mutex);

	return count;
}

static ssize_t show_history_size(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", odd.history_size);
}

static ssize_t store_history_size(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int size;

	ret = sscanf(buf, "%u", &size);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&odd.mutex);

	if (size >= 1) {
		odd.history_size = size;
		odd.low_load_cnt = 0;
	} else {
		return -EINVAL;
	}

	mutex_unlock(&odd.mutex);

	return count;
}

static ssize_t show_load(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", odd.load);
}

static DEVICE_ATTR(down_threshold, 0644,
	show_down_threshold, store_down_threshold);
static DEVICE_ATTR(up_threshold, 0644,
	show_up_threshold, store_up_threshold);
static DEVICE_ATTR(history_size, 0644,
	show_history_size, store_history_size);
static DEVICE_ATTR(load, 0444,
	show_load, NULL);

static struct attribute *on3demand_attributes[] = {
	&dev_attr_down_threshold.attr,
	&dev_attr_up_threshold.attr,
	&dev_attr_history_size.attr,
	&dev_attr_load.attr,
	NULL
};

static struct attribute_group on3demand_attr_group = {
	.attrs = on3demand_attributes,
	.name = "on3demand",
};
/************************ end sysfs interface ************************/

int on3demand_init(void)
{
	int ret;

	mutex_init(&odd.mutex);

	ret = sgxfreq_register_governor(&on3demand_gov);
	if (ret)
		return ret;

	return 0;
}

int on3demand_deinit(void)
{
	return 0;
}

static int on3demand_start(struct sgxfreq_sgx_data *data)
{
	int ret;

	odd.load = 0;
	odd.up_threshold = ON3DEMAND_DEFAULT_UP_THRESHOLD;
	odd.down_threshold = ON3DEMAND_DEFAULT_DOWN_THRESHOLD;
	odd.history_size = ON3DEMAND_DEFAULT_HISTORY_SIZE_THRESHOLD;
	odd.prev_total_active = 0;
	odd.prev_total_idle = 0;
	odd.low_load_cnt = 0;
	odd.poll_interval = ON3DEMAND_DEFAULT_POLL_INTERVAL;
	odd.polling_enabled = false;

	INIT_DELAYED_WORK(&odd.work, on3demand_timeout);

	ret = sysfs_create_group(sgxfreq_kobj, &on3demand_attr_group);
	if (ret)
		return ret;

	return 0;
}

static void on3demand_stop(void)
{
	cancel_delayed_work_sync(&odd.work);
	sysfs_remove_group(sgxfreq_kobj, &on3demand_attr_group);
}

static void on3demand_predict(void)
{
	static unsigned short first_sample = 1;
	unsigned long total_active, total_idle;
	unsigned long freq;

	if (first_sample == 1) {
		first_sample = 0;
		odd.prev_total_active = sgxfreq_get_total_active_time();
		odd.prev_total_idle = sgxfreq_get_total_idle_time();
		return;
	}

	/* Sample new active and idle times */
	total_active = sgxfreq_get_total_active_time();
	total_idle = sgxfreq_get_total_idle_time();

	/* Compute load */
	odd.delta_active = __delta32(total_active, odd.prev_total_active);
	odd.delta_idle = __delta32(total_idle, odd.prev_total_idle);

	/*
	 * If SGX was active for longer than frame display time (1/fps),
	 * scale to highest possible frequency.
	 */
	if (odd.delta_active > ON3DEMAND_FRAME_DONE_DEADLINE_MS) {
		odd.low_load_cnt = 0;
		sgxfreq_set_freq_request(sgxfreq_get_freq_max());
	}

	if ((odd.delta_active + odd.delta_idle))
		odd.load = (100 * odd.delta_active / (odd.delta_active + odd.delta_idle));

	odd.prev_total_active = total_active;
	odd.prev_total_idle = total_idle;

	/* Scale GPU frequency on purpose */
	if (odd.load >= odd.up_threshold) {
		odd.low_load_cnt = 0;
		sgxfreq_set_freq_request(sgxfreq_get_freq_max());
	} else if (odd.load <= odd.down_threshold) {
		if (odd.low_load_cnt == odd.history_size) {
			/* Convert load to frequency */
			freq = (sgxfreq_get_freq() * odd.load) / 100;
			sgxfreq_set_freq_request(freq);
			odd.low_load_cnt = 0;
		} else {
			odd.low_load_cnt++;
		}
	} else {
		odd.low_load_cnt = 0;
	}
}


static void on3demand_active(void)
{
	if (!odd.polling_enabled) {
		sgxfreq_set_freq_request(sgxfreq_get_freq_max());
		odd.low_load_cnt = 0;
		odd.polling_enabled = true;
		schedule_delayed_work(&odd.work, odd.poll_interval * HZ/1000);
	}

}

static void on3demand_frame_done(void)
{
	if (odd.polling_enabled) {
		cancel_delayed_work_sync(&odd.work);
		schedule_delayed_work(&odd.work, odd.poll_interval * HZ/1000);
	}
	on3demand_predict();
}

static void on3demand_timeout(struct work_struct *work)
{
	/*
	 * If sgx was idle all throughout timer disable polling and
	 * enable it on next sgx active event
	 */
	if (!odd.delta_active) {
		sgxfreq_set_freq_request(sgxfreq_get_freq_min());
		odd.low_load_cnt = 0;
		odd.polling_enabled = false;
	} else {
		on3demand_predict();
		odd.polling_enabled = true;
		schedule_delayed_work(&odd.work, odd.poll_interval * HZ/1000);
	}
}
