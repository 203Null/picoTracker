#include "Application/Instruments/CommandList.h"
#include "doctest/doctest.h"

TEST_CASE("Command coarse increments reach and saturate at VOL") {
  auto command = FourCC(FourCC::InstrumentCommandNone);
  for (unsigned step = 0; step < 64; ++step)
    command = CommandList::GetNextAlpha(command);
  CHECK(command == FourCC::InstrumentCommandVolume);
  CHECK(CommandList::GetNextAlpha(FourCC::InstrumentCommandTempo) ==
        FourCC::InstrumentCommandVelocity);
  CHECK(CommandList::GetNextAlpha(FourCC::InstrumentCommandVelocity) ==
        FourCC::InstrumentCommandVolume);
  CHECK(CommandList::GetNextAlpha(command) == command);
  CHECK(CommandList::GetNext(FourCC::InstrumentCommandVelocity) == command);
  CHECK(CommandList::GetPrev(command) == FourCC::InstrumentCommandVelocity);
}
