/*  Seed Farmer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_SeedFarmer_H
#define PokemonAutomation_PokemonFRLG_SeedFarmer_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "Common/Cpp/Options/TextEditOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonTools/Options/LanguageOCROption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "Pokemon/Pokemon_StatsCalculation.h"
#include "Pokemon/Pokemon_AdvRng.h"
#include "PokemonFRLG_BlindNavigation.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_RngDisplays.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

class SeedFarmer_Descriptor : public SingleSwitchProgramDescriptor{
public:
    SeedFarmer_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class SeedFarmer : public SingleSwitchProgramInstance{
public:
    SeedFarmer();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext &context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    enum class GameVersion{
        firered,
        leafgreen
    };

    enum class AudioSetting{
        mono,
        stereo
    };

    std::vector<std::pair<uint64_t, uint16_t>> flip_seed_map(const std::map<uint16_t, std::vector<uint64_t>>& seed_to_times);
    JsonValue seeds_to_json(
        const GameVersion& game_version, 
        const Language& language, 
        const AudioSetting& audio,
        const SeedButton& seed_button, 
        const BlackoutButton& extra_button, 
        const std::vector<std::pair<uint64_t, uint16_t>>& seed_list
    );
    void save_seeds(
        const std::string& output_json_filename,
        const GameVersion& game_version, 
        const Language& language, 
        const AudioSetting& audio,
        const SeedButton& seed_button, 
        const BlackoutButton& extra_button, 
        const std::map<uint16_t, std::vector<uint64_t>>& seed_to_times
    );

    StringOption FILE_NAME;

    SectionDividerOption m_calibration_displays;
    StringOption CURRENT_SEED_DELAY;
    RngFilterDisplay RNG_FILTERS;
    RngCalibrationDisplay RNG_CALIBRATION;

    SectionDividerOption m_game_info;
    EnumDropdownOption<GameVersion> GAME_VERSION;
    OCR::LanguageOCROption LANGUAGE;

    SectionDividerOption m_target_settings;
    EnumDropdownOption<PokemonFRLG_RngTarget> TARGET;
    EnumDropdownOption<AudioSetting> AUDIO_SETTING;
    EnumDropdownOption<SeedButton> SEED_BUTTON;
    EnumDropdownOption<BlackoutButton> EXTRA_BUTTON;
    SimpleIntegerOption<uint64_t> MIN_SEED_DELAY;
    SimpleIntegerOption<uint64_t> MAX_SEED_DELAY;
    SimpleIntegerOption<uint64_t>ADVANCES;

    SectionDividerOption m_program_settings;
    SimpleIntegerOption<uint64_t>DELAY_STEP_SIZE;
    SimpleIntegerOption<uint64_t>SAVE_EVERY;
    SimpleIntegerOption<uint64_t> MAX_RARE_CANDIES;
    SimpleIntegerOption<uint8_t> PROFILE;

    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};

}
}
}
#endif
