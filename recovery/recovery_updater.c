/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "edify/expr.h"
#include "bootloader.h"

Value* WriteBootloaderFn(const char* name, State* state, int argc, Expr* argv[])
{
    int result = -1;
    Value* img;
    Value* xloader_loc;
    Value* sbl_loc;

    if (argc != 3) {
        return ErrorAbort(state, "%s() expects 3 args, got %d", name, argc);
    }

    if (ReadValueArgs(state, argv, 3, &img, &xloader_loc, &sbl_loc) < 0) {
        return NULL;
    }

    if(img->type != VAL_BLOB ||
       xloader_loc->type != VAL_STRING ||
       sbl_loc->type != VAL_STRING) {
      FreeValue(img);
      FreeValue(xloader_loc);
      FreeValue(sbl_loc);
      return ErrorAbort(state, "%s(): argument types are incorrect", name);
    }

    result = update_bootloader(img->data, img->size,
                               xloader_loc->data, sbl_loc->data);
    FreeValue(img);
    FreeValue(xloader_loc);
    FreeValue(sbl_loc);
    return StringValue(strdup(result == 0 ? "t" : ""));
}

void Register_librecovery_updater_tuna() {
    fprintf(stderr, "installing samsung updater extensions\n");

    RegisterFunction("samsung.write_bootloader", WriteBootloaderFn);
}
