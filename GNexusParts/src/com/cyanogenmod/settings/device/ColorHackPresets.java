/*
 * Copyright (C) 2011 The CyanogenMod Project
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

package com.cyanogenmod.settings.device;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.preference.DialogPreference;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.util.Log;



/**
 * Special preference type that allows to set a pre-configuration of Color and Gamma
 * Value on Nexus Devices.
 */
public class ColorHackPresets extends DialogPreference implements OnClickListener {

    private static final String TAG = "PRESETS...";

    private static final String[] FILE_PATH_GAMMA = new String[] {
            "/sys/class/misc/samoled_color/red_v1_offset",
            "/sys/class/misc/samoled_color/green_v1_offset",
            "/sys/class/misc/samoled_color/blue_v1_offset",
            "/sys/devices/platform/omapdss/manager0/gamma"
    };

    private static final String[] FILE_PATH_MULTI = new String[] {
            "/sys/class/misc/samoled_color/red_multiplier",
            "/sys/class/misc/samoled_color/green_multiplier",
            "/sys/class/misc/samoled_color/blue_multiplier"
    };

    // Align MAX_VALUE with Voodoo Control settings
    private static final int MAX_VALUE = 2000000000;
    

    public ColorHackPresets(Context context, AttributeSet attrs) {
        super(context, attrs);

        setDialogLayoutResource(R.layout.preference_colorgamma_presets);
    }

    @Override
    protected void onBindDialogView(View view) {
        super.onBindDialogView(view);
        SetupButtonClickListeners(view);
    }

    private void SetupButtonClickListeners(View view) {
        Button[] mPresets = new Button[6];

        mPresets[0] = (Button)view.findViewById(R.id.btnPreset1);
        mPresets[1] = (Button)view.findViewById(R.id.btnPreset2);
        mPresets[2] = (Button)view.findViewById(R.id.btnPreset3);
        mPresets[3] = (Button)view.findViewById(R.id.btnPreset4);
        mPresets[4] = (Button)view.findViewById(R.id.btnPreset5);
        mPresets[5] = (Button)view.findViewById(R.id.btnPreset6);
        for (int i = 0; i < 6; i++) {
            mPresets[i].setOnClickListener(this);
        }
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        super.onDialogClosed(positiveResult);

    }

    
    /**
     * Check whether the running kernel supports color/gamma tuning or not.
     * 
     * @return Whether color/gamma tuning is supported or not
     */
    public static boolean isSupported() {
        boolean supported = true;
        for (String filePath : FILE_PATH_MULTI) {
            if (!Utils.fileExists(filePath)) {
                supported = false;
            }
        }
        for (String filePath : FILE_PATH_GAMMA) {
            if (!Utils.fileExists(filePath)) {
                supported = false;
            }
        }
        return supported;
    }

    public void onClick(View v) {
        switch(v.getId()){
            case R.id.btnPreset1:
                    Preset1();
                    break;
            case R.id.btnPreset2:
                    Preset2();
                    break;
            case R.id.btnPreset3:
                    Preset3();
                    break;
            case R.id.btnPreset4:
                    Preset4();
                    break;
            case R.id.btnPreset5:
                    Preset5();
                    break;
            case R.id.btnPreset6:
                    Preset6();
                    break;                 
        }
    }


    private void WriteMultiplier(Double fValue , int iPos) {
        int iValue = (int) ((double) MAX_VALUE * fValue);
        int iValue2;
        Utils.writeColor(FILE_PATH_MULTI[iPos], iValue);
        
        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        iValue2 = sharedPrefs.getInt(FILE_PATH_MULTI[iPos], iValue);
        //storing to SharedPreferences because of reloaded @ startup
        Editor editor = getEditor();
        editor.putInt(FILE_PATH_MULTI[iPos], iValue);
        editor.commit();
        Log.d(TAG, "Changing ColorMultiplier " +iPos+ " from:" +iValue2+ " to: " +iValue);
    }

    
    private void WriteGamma(int iValue , int iPos) {
        int iValue2;
        Utils.writeValue(FILE_PATH_GAMMA[iPos], String.valueOf((long) iValue));
        
        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        iValue2 = sharedPrefs.getInt(FILE_PATH_GAMMA[iPos], iValue);
        //storing to SharedPreferences because of reloaded @ startup
        Editor editor = getEditor();
        editor.putInt(FILE_PATH_GAMMA[iPos], iValue);
        editor.commit();
        Log.d(TAG, "Changing GammaValue " +iPos+ " from:" +iValue2+ " to: " +iValue);
    }
    

    private void Preset1() {
        WriteMultiplier(0.5, 0);
        WriteMultiplier(0.5, 1);
        WriteMultiplier(0.5, 2);
        WriteGamma(0, 0);
        WriteGamma(0, 1);
        WriteGamma(0, 2);
        WriteGamma(0, 3);
    }

    private void Preset2() {
        WriteMultiplier(1.0, 0);
        WriteMultiplier(1.0, 1);
        WriteMultiplier(1.0, 2);
        WriteGamma(0, 0);
        WriteGamma(0, 1);
        WriteGamma(0, 2);
        WriteGamma(0, 3);
    }

    private void Preset3() {
        WriteMultiplier(0.35, 0);
        WriteMultiplier(0.38, 1);
        WriteMultiplier(0.5, 2);
        WriteGamma(0, 0);
        WriteGamma(0, 1);
        WriteGamma(0, 2);
        WriteGamma(0, 3);
    }

    private void Preset4() {
        WriteMultiplier(0.7231, 0);
        WriteMultiplier(0.7016, 1);
        WriteMultiplier(0.6532, 2);
        WriteGamma(-31, 0);
        WriteGamma(-30, 1);
        WriteGamma(-14, 2);
        WriteGamma(0, 3);
    }

    private void Preset5() {
        WriteMultiplier(0.6666, 0);
        WriteMultiplier(0.6666, 1);
        WriteMultiplier(0.8333, 2);
        WriteGamma(-44, 0);
        WriteGamma(-44, 1);
        WriteGamma(-7, 2);
        WriteGamma(0, 3);
    }

    private void Preset6() {
        WriteMultiplier(0.45, 0);
        WriteMultiplier(0.48, 1);
        WriteMultiplier(0.5, 2);
        WriteGamma(-4, 0);
        WriteGamma(0, 1);
        WriteGamma(5, 2);
        WriteGamma(0, 3);
    }

}
