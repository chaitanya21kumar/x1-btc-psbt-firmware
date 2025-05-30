// Copyright 2019 Chaitanya Kumar
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _UI_CONFIRM_BUTTON_H_
#define _UI_CONFIRM_BUTTON_H_

#include "icon_button.h"
#include <ui/component.h>

#include <stdbool.h>

/**
 * Creates a confirm button. Confirming emits the EVENT_CONFIRM event.
 * @param[in] longtouch if true, hold gesture is required, otherwise a simple tap.
 * @param[in] button_type if not longtouch, defines the icon to show.
 */
component_t* confirm_button_create(bool longtouch, icon_button_type_t button_type);

#endif
