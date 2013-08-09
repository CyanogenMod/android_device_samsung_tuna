/*
 * Copyright (C) 2013 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.cyanogenmod.hardware;

import org.cyanogenmod.hardware.util.FileUtils;
import java.io.File;

public class DisplayColorCalibration {

    private static final String[] FILE_PATH = new String[] {
            "/sys/class/misc/samoled_color/red_multiplier",
            "/sys/class/misc/samoled_color/green_multiplier",
            "/sys/class/misc/samoled_color/blue_multiplier"
    };

    // Align MAX_VALUE with Voodoo Control settings
    private static final int MAX_VALUE = 2000000000;

    public static boolean isSupported() {
        for (String i : FILE_PATH) {
            if (!new File(i).exists()) {
                return false;
            }
        }
        return true;
    }

    public static int getMaxValue()  {
        return MAX_VALUE;
    }
    public static int getMinValue()  {
        return 0;
    }
    public static String getCurColors()  {
        StringBuilder colors = new StringBuilder();
        for (String filePath : FILE_PATH) {
            colors.append(FileUtils.readOneLine(filePath)).append(" ");
        }
        return colors.toString();
    }
    public static boolean setColors(String colors)  {
        String[] colorsSplit = colors.split(" ");
        boolean result = true;
        for (int i = 0; i < 3; i++) {
            String currentFile = FILE_PATH[i];
            result &= FileUtils.writeLine(currentFile, colorsSplit[i]);
        }
        return result;
    }
}
