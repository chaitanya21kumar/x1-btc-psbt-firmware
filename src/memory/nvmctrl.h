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

#ifndef __NVMCTRL_H
#define __NVMCTRL_H

#include <stdint.h>

/**
 * Writes a command to the NVM controller, and
 * waits for it to be completed.
 *
 * @param[in] cmd Command the NVM controller must execute.
 */
void nvmctrl_exec_cmd(uint16_t cmd);

#endif // __NVMCTRL_H
