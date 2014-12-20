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

#include <linux/opp.h>
#include <plat/gpu.h>
#include "sgxfreq.h"

static struct sgxfreq_data {
	int freq_cnt;
	unsigned long *freq_list;
	unsigned long freq;
	unsigned long freq_request;
	unsigned long freq_limit;
	unsigned long total_idle_time;
	unsigned long total_active_time;
	struct mutex freq_mutex;
	struct list_head gov_list;
	struct sgxfreq_governor *gov;
	struct mutex gov_mutex;
	struct sgxfreq_sgx_data sgx_data;
	struct device *dev;
	struct gpu_platform_data *pdata;
} sfd;

/* Governor init/deinit functions */
int onoff_init(void);
int onoff_deinit(void);
int activeidle_init(void);
int activeidle_deinit(void);
int on3demand_init(void);
int on3demand_deinit(void);
int userspace_init(void);
int userspace_deinit(void);


typedef int sgxfreq_gov_init_t(void);
sgxfreq_gov_init_t *sgxfreq_gov_init[] = {
	onoff_init,
	activeidle_init,
	on3demand_init,
	userspace_init,
	NULL,
};

typedef int sgxfreq_gov_deinit_t(void);
sgxfreq_gov_deinit_t *sgxfreq_gov_deinit[] = {
	onoff_deinit,
	activeidle_deinit,
	on3demand_deinit,
	userspace_deinit,
	NULL,
};

#define SGXFREQ_DEFAULT_GOV_NAME "on3demand"
static unsigned long _idle_curr_time;
static unsigned long _idle_prev_time;
static unsigned long _active_curr_time;
static unsigned long _active_prev_time;

#if defined(CONFIG_THERMAL_FRAMEWORK)
int cool_init(void);
void cool_deinit(void);
#endif

/*********************** begin sysfs interface ***********************/

struct kobject *sgxfreq_kobj;

static ssize_t show_frequency_list(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int i;
	ssize_t count = 0;

	for (i = 0; i < sfd.freq_cnt; i++)
		count += sprintf(&buf[count], "%lu ", sfd.freq_list[i]);
	count += sprintf(&buf[count], "\n");

	return count;
}

static ssize_t show_frequency_request(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%lu\n", sfd.freq_request);
}

static ssize_t show_frequency_limit(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%lu\n", sfd.freq_limit);
}

/*
 * sysfs interface to store sgxfreq frequency limit
 * author: ketut.kumajaya@gmail.com
*/
static ssize_t store_frequency_limit(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned long freq_limit;

	if (kstrtoul(buf, 0, &freq_limit)) {
		pr_err("sgxfreq: failed storing frequency_limit\n");
		return -EINVAL;
	}

	freq_limit = sgxfreq_set_freq_limit(freq_limit);
	pr_info("sgxfreq: frequency_limit=%lu\n", freq_limit);
	return count;
}

static ssize_t show_frequency(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%lu\n", sfd.freq);
}

static ssize_t show_stat(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "gpu %lu %lu\n",
		sfd.total_active_time, sfd.total_idle_time);
}

static ssize_t show_governor_list(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t i = 0;
	struct sgxfreq_governor *t;

	list_for_each_entry(t, &sfd.gov_list, governor_list) {
		if (i >= (ssize_t) ((PAGE_SIZE / sizeof(char))
		    - (SGXFREQ_NAME_LEN + 2)))
			goto out;
		i += scnprintf(&buf[i], SGXFREQ_NAME_LEN, "%s ", t->name);
	}
out:
	i += sprintf(&buf[i], "\n");
	return i;
}

static ssize_t show_governor(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	if (sfd.gov)
		return scnprintf(buf, SGXFREQ_NAME_LEN, "%s\n", sfd.gov->name);

	return sprintf(buf, "\n");
}

static ssize_t store_governor(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	int ret;
	char name[16];

	ret = sscanf(buf, "%15s", name);
	if (ret != 1)
		return -EINVAL;

	ret = sgxfreq_set_governor(name);
	if (ret)
		return ret;
	else
		return count;
}

static DEVICE_ATTR(frequency_list, 0444, show_frequency_list, NULL);
static DEVICE_ATTR(frequency_request, 0444, show_frequency_request, NULL);
static DEVICE_ATTR(frequency_limit, 0644, show_frequency_limit, store_frequency_limit);
static DEVICE_ATTR(frequency, 0444, show_frequency, NULL);
static DEVICE_ATTR(governor_list, 0444, show_governor_list, NULL);
static DEVICE_ATTR(governor, 0644, show_governor, store_governor);
static DEVICE_ATTR(stat, 0444, show_stat, NULL);

static const struct attribute *sgxfreq_attributes[] = {
	&dev_attr_frequency_list.attr,
	&dev_attr_frequency_request.attr,
	&dev_attr_frequency_limit.attr,
	&dev_attr_frequency.attr,
	&dev_attr_governor_list.attr,
	&dev_attr_governor.attr,
	&dev_attr_stat.attr,
	NULL
};

/************************ end sysfs interface ************************/

static void __set_freq(void)
{
	unsigned long freq;

	freq = min(sfd.freq_request, sfd.freq_limit);
	if (freq != sfd.freq) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
		sfd.pdata->device_scale(sfd.dev, sfd.dev, freq);
#else
		sfd.pdata->device_scale(sfd.dev, freq);
#endif
		sfd.freq = freq;
	}
}

static struct sgxfreq_governor *__find_governor(const char *name)
{
        struct sgxfreq_governor *t;

        list_for_each_entry(t, &sfd.gov_list, governor_list)
                if (!strnicmp(name, t->name, SGXFREQ_NAME_LEN))
                        return t;

        return NULL;
}

static void __update_timing_info(bool active)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	if(active)
	{
		if(sfd.sgx_data.active == true) {
			_active_curr_time = __tv2msec(tv);
			sfd.total_active_time += __delta32(
					_active_curr_time, _active_prev_time);
			SGXFREQ_TRACE("A->A TA:= %lums \tdA: %lums \tTI: %lums \tdI: %lums\n",
					sfd.total_active_time,
					__delta32(_active_curr_time, _active_prev_time),
					sfd.total_active_time,
					(unsigned long)0);
			_active_prev_time = _active_curr_time;
		} else {
			_idle_curr_time = __tv2msec(tv);
			_active_prev_time = _idle_curr_time;
			sfd.total_idle_time +=
					__delta32(_idle_curr_time, _idle_prev_time);
			SGXFREQ_TRACE("I->A TA:= %lums \tdA: %lums \tTI: %lums \tdI: %lums\n",
					sfd.total_active_time,
					(unsigned long)0,
					sfd.total_idle_time,
					__delta32(_idle_curr_time, _idle_prev_time));
		}
	} else {
		if(sfd.sgx_data.active == true)
		{
			_idle_prev_time = _active_curr_time = __tv2msec(tv);
			sfd.total_active_time +=
					__delta32(_active_curr_time, _active_prev_time);
			SGXFREQ_TRACE("A->I TA:= %lums \tdA: %lums \tTI: %lums \tdI: %lums\n",
					sfd.total_active_time,
					__delta32(_active_curr_time, _active_prev_time),
					sfd.total_active_time,
					(unsigned long)0);
		}
		else
		{
			_idle_curr_time = __tv2msec(tv);
			sfd.total_idle_time += __delta32(
					_idle_curr_time, _idle_prev_time);
			SGXFREQ_TRACE("I->I TA:= %lums \tdA: %lums \tTI: %lums \tdI: %lums\n",
					sfd.total_active_time,
					(unsigned long)0,
					sfd.total_idle_time,
					__delta32(_idle_curr_time, _idle_prev_time));
			_idle_prev_time = _idle_curr_time;
		}
	}
}

int sgxfreq_init(struct device *dev)
{
	int i, ret;
	unsigned long freq;
	struct opp *opp;
	struct timeval tv;

	sfd.dev = dev;
	if (!sfd.dev)
		return -EINVAL;

	sfd.pdata = (struct gpu_platform_data *)dev->platform_data;
	if (!sfd.pdata ||
	    !sfd.pdata->opp_get_opp_count ||
	    !sfd.pdata->opp_find_freq_ceil ||
	    !sfd.pdata->device_scale)
		return -EINVAL;

	rcu_read_lock();

	sfd.freq_cnt = sfd.pdata->opp_get_opp_count(dev);
	if (sfd.freq_cnt < 1) {
		rcu_read_unlock();
		return -ENODEV;
	}

	sfd.freq_list = kmalloc(sfd.freq_cnt * sizeof(unsigned long), GFP_ATOMIC);
        if (!sfd.freq_list) {
		rcu_read_unlock();
		return -ENOMEM;
	}

	freq = 0;
	for (i = 0; i < sfd.freq_cnt; i++) {
		opp = sfd.pdata->opp_find_freq_ceil(dev, &freq);
		if (IS_ERR_OR_NULL(opp)) {
			rcu_read_unlock();
			kfree(sfd.freq_list);
			return -ENODEV;
		}
		sfd.freq_list[i] = freq;
		freq++;
	}
	rcu_read_unlock();

	mutex_init(&sfd.freq_mutex);
	sfd.freq_limit = sfd.freq_list[sfd.freq_cnt - 1];
	sgxfreq_set_freq_request(sfd.freq_list[sfd.freq_cnt - 1]);
	sfd.sgx_data.clk_on = false;
	sfd.sgx_data.active = false;

	mutex_init(&sfd.gov_mutex);
	INIT_LIST_HEAD(&sfd.gov_list);

	sgxfreq_kobj = kobject_create_and_add("sgxfreq", &sfd.dev->kobj);
	ret = sysfs_create_files(sgxfreq_kobj, sgxfreq_attributes);
	if (ret) {
		kfree(sfd.freq_list);
		return ret;
	}

#if defined(CONFIG_THERMAL_FRAMEWORK)
	cool_init();
#endif

	for (i = 0; sgxfreq_gov_init[i] != NULL; i++)
		sgxfreq_gov_init[i]();

	if (sgxfreq_set_governor(SGXFREQ_DEFAULT_GOV_NAME)) {
		kfree(sfd.freq_list);
		return -ENODEV;
	}
	do_gettimeofday(&tv);
	_idle_prev_time = _active_curr_time = _idle_curr_time =
		_active_prev_time = __tv2msec(tv);

	return 0;
}

int sgxfreq_deinit(void)
{
	int i;

	sgxfreq_set_governor(NULL);

	sgxfreq_set_freq_request(sfd.freq_list[0]);

#if defined(CONFIG_THERMAL_FRAMEWORK)
	cool_deinit();
#endif

	for (i = 0; sgxfreq_gov_deinit[i] != NULL; i++)
		sgxfreq_gov_deinit[i]();

	sysfs_remove_files(sgxfreq_kobj, sgxfreq_attributes);
	kobject_put(sgxfreq_kobj);

	kfree(sfd.freq_list);

	return 0;
}

int sgxfreq_register_governor(struct sgxfreq_governor *governor)
{
	if (!governor)
		return -EINVAL;

	list_add(&governor->governor_list, &sfd.gov_list);

	return 0;
}

void sgxfreq_unregister_governor(struct sgxfreq_governor *governor)
{
	if (!governor)
		return;

	list_del(&governor->governor_list);
}

int sgxfreq_set_governor(const char *name)
{
	int ret = 0;
	struct sgxfreq_governor *new_gov = 0;

	if (name) {
		new_gov = __find_governor(name);
		if (!new_gov)
			return -EINVAL;
	}

	mutex_lock(&sfd.gov_mutex);

	if (sfd.gov && sfd.gov->gov_stop)
		sfd.gov->gov_stop();

	if (new_gov && new_gov->gov_start)
		ret = new_gov->gov_start(&sfd.sgx_data);

	if (ret) {
		if (sfd.gov && sfd.gov->gov_start)
			sfd.gov->gov_start(&sfd.sgx_data);
		return -ENODEV;
	}
	sfd.gov = new_gov;

	mutex_unlock(&sfd.gov_mutex);

	return 0;
}

int sgxfreq_get_freq_list(unsigned long **pfreq_list)
{
	*pfreq_list = sfd.freq_list;

	return sfd.freq_cnt;
}

unsigned long sgxfreq_get_freq_min(void)
{
	return sfd.freq_list[0];
}

unsigned long sgxfreq_get_freq_max(void)
{
	return sfd.freq_list[sfd.freq_cnt - 1];
}

unsigned long sgxfreq_get_freq_floor(unsigned long freq)
{
	int i;
	unsigned long f = 0;

	for (i = sfd.freq_cnt - 1; i >= 0; i--) {
		f = sfd.freq_list[i];
		if (f <= freq)
			return f;
	}

	return f;
}

unsigned long sgxfreq_get_freq_ceil(unsigned long freq)
{
	int i;
	unsigned long f = 0;

	for (i = 0; i < sfd.freq_cnt; i++) {
		f = sfd.freq_list[i];
		if (f >= freq)
			return f;
	}

	return f;
}

unsigned long sgxfreq_get_freq(void)
{
	return sfd.freq;
}

unsigned long sgxfreq_get_freq_request(void)
{
	return sfd.freq_request;
}

unsigned long sgxfreq_get_freq_limit(void)
{
	return sfd.freq_limit;
}

unsigned long sgxfreq_set_freq_request(unsigned long freq_request)
{
	freq_request = sgxfreq_get_freq_ceil(freq_request);

	mutex_lock(&sfd.freq_mutex);

	sfd.freq_request = freq_request;
	__set_freq();

	mutex_unlock(&sfd.freq_mutex);

	return freq_request;
}

unsigned long sgxfreq_set_freq_limit(unsigned long freq_limit)
{
	freq_limit = sgxfreq_get_freq_ceil(freq_limit);

	mutex_lock(&sfd.freq_mutex);

	sfd.freq_limit = freq_limit;
	__set_freq();

	mutex_unlock(&sfd.freq_mutex);

	return freq_limit;
}

unsigned long sgxfreq_get_total_active_time(void)
{
	__update_timing_info(sfd.sgx_data.active);
	return sfd.total_active_time;
}

unsigned long sgxfreq_get_total_idle_time(void)
{
	__update_timing_info(sfd.sgx_data.active);
	return sfd.total_idle_time;
}

/*
 * sgx_clk_on, sgx_clk_off, sgx_active, and sgx_idle notifications are
 * serialized by power lock. governor notif calls need sync with governor
 * setting.
 */
void sgxfreq_notif_sgx_clk_on(void)
{
	sfd.sgx_data.clk_on = true;

	mutex_lock(&sfd.gov_mutex);

	if (sfd.gov && sfd.gov->sgx_clk_on)
		sfd.gov->sgx_clk_on();

	mutex_unlock(&sfd.gov_mutex);
}

void sgxfreq_notif_sgx_clk_off(void)
{
	sfd.sgx_data.clk_on = false;

	mutex_lock(&sfd.gov_mutex);

	if (sfd.gov && sfd.gov->sgx_clk_off)
		sfd.gov->sgx_clk_off();

	mutex_unlock(&sfd.gov_mutex);
}


void sgxfreq_notif_sgx_active(void)
{
	__update_timing_info(true);

	sfd.sgx_data.active = true;

	mutex_lock(&sfd.gov_mutex);

	if (sfd.gov && sfd.gov->sgx_active)
		sfd.gov->sgx_active();

	mutex_unlock(&sfd.gov_mutex);

}

void sgxfreq_notif_sgx_idle(void)
{

	__update_timing_info(false);

	sfd.sgx_data.active = false;

	mutex_lock(&sfd.gov_mutex);

	if (sfd.gov && sfd.gov->sgx_idle)
		sfd.gov->sgx_idle();

	mutex_unlock(&sfd.gov_mutex);
}

void sgxfreq_notif_sgx_frame_done(void)
{
	mutex_lock(&sfd.gov_mutex);

	if (sfd.gov && sfd.gov->sgx_frame_done)
		sfd.gov->sgx_frame_done();

	mutex_unlock(&sfd.gov_mutex);
}
