// Copyright 2017 Neverware
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "gpt_pal.h"

// We use gdisk to clean up the GPT such that Windows is happy writing to
// the disk
#include "../gdisk/gpt.h"
#include "../gdisk/parttypes.h"

class PalData : public GPTData {
 public:
  PalData();
  WhichToUse UseWhichPartitions(void) override;
  void ClearDisk();
};

PalData::PalData() : GPTData() {}

WhichToUse PalData::UseWhichPartitions(void) {
  // The disk may be in a weird state, but we are about to reformat it.
  // Just always use a new partition table.
  return use_new;
}

void PalData::ClearDisk() {
  int newPartNum = 1;
  uint64_t low = FindFirstInLargest();
  Align(&low);
  uint64_t high = FindLastInFree(low);
  uint64_t startSector = low;
  uint64_t endSector = high;
  CreatePartition(newPartNum, startSector, endSector);
  partitions[0].SetType(0x0b00);  // make it fat32
  // arg is 'quiet'
  SaveGPTData(true);
}

bool clearMbrGpt(const char* physical_path) {
  std::string physical_path_str(physical_path);
  PalData gptdata;
  // set the physical path for this GPT object to act on
  gptdata.LoadPartitions(std::string(physical_path));
  int quiet = true;
  // attempt to fix any gpt/mbr problems by setting to a sane, empty state
  gptdata.SaveGPTData(quiet);
  int problems = gptdata.Verify();
  if (problems > 0) {
    return false;
  } else {
    return true;
  }
}

bool makeEmptyPartition(const char* physical_path) {
  char* newPartInfo;
  PalData gptdata;
  gptdata.LoadPartitions(std::string(physical_path));
  gptdata.ClearDisk();
  return true;

  int problems = gptdata.Verify();
  free(newPartInfo);
  // TODO: unclear if we care about problems in this regard
  if (problems > 0) {
    return true;
  } else {
    return false;
  }
}
