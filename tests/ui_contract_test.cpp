#include "ui/UiContract.hpp"
#include <cassert>

int main() {
    static_assert(ui::kUiContractVersion == 1);
    static_assert(static_cast<unsigned>(ui::ModeId::NowPlaying) == 1);
    static_assert(static_cast<unsigned>(ui::ModeId::Spectrum) == 2);
    static_assert(static_cast<unsigned>(ui::ModeId::DeskBoard) == 5);
    static_assert(static_cast<unsigned>(ui::SpectrumStyleId::Bars) == 0);
    static_assert(static_cast<unsigned>(ui::SpectrumStyleId::CymaticMesh) == 12);
    static_assert(ui::kVisibleModeOrder.size() == 5);

    ui::UiCapabilities capabilities;
    capabilities.modeMask &= ~ui::modeBit(ui::ModeId::Clock);
    capabilities.spectrumStyleMask &= ~ui::styleBit(ui::SpectrumStyleId::Halo);
    assert(ui::nextSupportedMode(ui::ModeId::Spectrum, capabilities) ==
           ui::ModeId::Market);
    assert(ui::nextSupportedSpectrumStyle(ui::SpectrumStyleId::Oscilloscope,
                                          capabilities) ==
           ui::SpectrumStyleId::Waterfall);
}
