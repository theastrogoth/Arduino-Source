/*  Seed Farmer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QFile>
#include <algorithm>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Language.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/StartupChecks/StartProgramChecks.h"
#include "Pokemon/Pokemon_Strings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_RngNavigation.h"
#include "PokemonFRLG_HardReset.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_SeedFarmer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


SeedFarmer_Descriptor::SeedFarmer_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:SeedFarmer",
        Pokemon::STRING_POKEMON + " FRLG", "Seed Farmer",
        "Programs/PokemonFRLG/SeedFarmer.html",
        "Automatically determine seeds for a range of seed delays.",
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct SeedFarmer_Descriptor::Stats : public StatsTracker{
    Stats()
        : resets(m_stats["Resets"])
        , seeds(m_stats["Seeds Farmed"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Resets");
        m_display_order.emplace_back("Seeds Farmed");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }
    std::atomic<uint64_t>& resets;
    std::atomic<uint64_t>& seeds;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> SeedFarmer_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

SeedFarmer::SeedFarmer()
    : FILE_NAME(
        false,
        "<b>File name:</b><br>"
        "Name of the JSON file to write.", 
        LockMode::LOCK_WHILE_RUNNING, 
        "UserSettings/seeds",
        "<name of JSON file>"
    )
    , m_calibration_displays(
        "<font size=4><b>Calibration Displays</b></font> — These will update automatically as the program runs"
    )
    , CURRENT_SEED_DELAY(
        false, "<b>Current Seed Delay:</b>",
        LockMode::READ_ONLY, "-",
        "-", false
    )
    , m_game_info(
        "<font size=4><b>Game Information</b></font>"
    )
    , GAME_VERSION(
        "<b>Game Version:</b>",
        {
            {GameVersion::firered, "firered", "FireRed"},
            {GameVersion::leafgreen, "leafgreen", "LeafGreen"}
        },
        LockMode::LOCK_WHILE_RUNNING,
        GameVersion::firered
    )
    , LANGUAGE(
        "<b>Game Language:</b>",
        {
            Language::English,
            Language::Japanese,
            Language::Spanish,
            Language::French,
            Language::German,
            Language::Italian,
        },
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , m_target_settings(
        "<font size=4><b>Target Settings</b></font> — Get these from an RNG search tool"
    )
    , TARGET(
        "<b>Target:</b>",
        {
            {PokemonFRLG_RngTarget::magikarp, "magikarp", "Magikarp"},
            {PokemonFRLG_RngTarget::hitmonchan, "hitmonchan", "Hitmonchan"},
            {PokemonFRLG_RngTarget::hitmonlee, "hitmonlee", "Hitmonlee"},
            {PokemonFRLG_RngTarget::eevee, "eevee", "Eevee"},
            {PokemonFRLG_RngTarget::lapras, "lapras", "Lapras"},
            {PokemonFRLG_RngTarget::omanyte, "omanyte", "Omanyte"},
            {PokemonFRLG_RngTarget::kabuto, "kabuto", "Kabuto"},
            {PokemonFRLG_RngTarget::aerodactyl, "aerodactyl", "Aerodactyl"},
            {PokemonFRLG_RngTarget::gamecornerabra, "gamecornerabra", "Game Corner Abra"},
            {PokemonFRLG_RngTarget::gamecornerclefairy, "gamecornerclefairy", "Game Corner Clefairy"},
            {PokemonFRLG_RngTarget::gamecornerdratinifr, "gamecornerdratinifr", "Game Corner Dratini (FireRed)"},
            {PokemonFRLG_RngTarget::gamecornerdratinilg, "gamecornerdratinilg", "Game Corner Dratini (LeafGreen)"},
            {PokemonFRLG_RngTarget::gamecornerscyther, "gamecornerscyther", "Game Corner Scyther"},
            {PokemonFRLG_RngTarget::gamecornerpinsir, "gamecornerpinsir", "Game Corner Pinsir"},
            {PokemonFRLG_RngTarget::gamecornerporygon, "gamecornerporygon", "Game Corner Porygon"},
            {PokemonFRLG_RngTarget::togepi, "togepi", "Togepi"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        PokemonFRLG_RngTarget::eevee
    )    
    , AUDIO_SETTING(
        "<b>Audio:</b>",
        {
            {AudioSetting::mono, "mono", "Mono"},
            {AudioSetting::stereo, "stereo", "Stereo"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        AudioSetting::mono
    )
    , SEED_BUTTON(
        "<b>Seed Button:</b>",
        {
            {SeedButton::A, "A", "A"},
            {SeedButton::Start, "Start", "Start"},
            {SeedButton::L, "L", "L (L=A)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        SeedButton::A
    )
    , EXTRA_BUTTON(
        "<b>Extra Button:</b><br>"
        "Additional button presses that affect the seed.",
        {
            {BlackoutButton::None, "None", "None"},
            {BlackoutButton::L, "L", "Blackout L"},
            {BlackoutButton::R, "R", "Blackout R"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        BlackoutButton::None
    )
    , MIN_SEED_DELAY(
        "<b>Minimum Seed Delay Time (ms):</b>",
        LockMode::LOCK_WHILE_RUNNING,
        30500, 30400 // default, min
    )
    , MAX_SEED_DELAY(
        "<b>Maximum Seed Delay Time (ms):</b>",
        LockMode::LOCK_WHILE_RUNNING,
        60000, 30500, 70000 // default, min, max
    )
    , ADVANCES(
        "<b>Advances:</b><br>The total number of RNG advances for your target.",
        LockMode::LOCK_WHILE_RUNNING,
        600, 520, 1000000000 // default, min, max
    )
    , m_program_settings(
        "<font size=4><b>Program Settings</b></font>"
    )
    , DELAY_STEP_SIZE(
        "<b>Seed Delay Step Size:</b><br>"
        "The amount to increment the seed delay every reset.<br>"
        "Lower values will result in more accurate result, but will cause the program to take longer",
        LockMode::LOCK_WHILE_RUNNING,
        4, 1, 16 // default, min, max
    )
    , SAVE_EVERY(
       "<b>Save Every ___ Resets:</b><br>"
        "The number of resets to perform before periodically writing farmed seeds to a JSON output.",
        LockMode::LOCK_WHILE_RUNNING,
        50, 1 // default, min 
    )
    , MAX_RARE_CANDIES(
        "<b>Max Rare Candies:</b><br>"
        "The number of rare candies in your bag. Make sure these are at the top position of the bag.<br>"
        "Rare candies used during calibration will be restored after resetting.",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 999 // default, min, max
    )
    , PROFILE(
        "<b>User Profile Position:</b><br>"
        "The position, from left to right, of the Switch profile with the FRLG save you'd like to use.<br>"
        "If this is set to 0, Switch 1 defaults to the last-used profile, while Switch 2 defaults to the first profile (position 1)",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 8 // default, min, max
    )
    , GO_HOME_WHEN_DONE(true)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
    })
{
    PA_ADD_OPTION(FILE_NAME);
    PA_ADD_OPTION(m_calibration_displays);
    PA_ADD_OPTION(CURRENT_SEED_DELAY);
    PA_ADD_OPTION(RNG_FILTERS);
    PA_ADD_OPTION(RNG_CALIBRATION);
    PA_ADD_OPTION(m_game_info);
    PA_ADD_OPTION(GAME_VERSION);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(m_target_settings);
    PA_ADD_OPTION(TARGET);
    PA_ADD_OPTION(AUDIO_SETTING);
    PA_ADD_OPTION(SEED_BUTTON);
    PA_ADD_OPTION(EXTRA_BUTTON);
    PA_ADD_OPTION(MIN_SEED_DELAY);
    PA_ADD_OPTION(MAX_SEED_DELAY);
    PA_ADD_OPTION(ADVANCES);
    PA_ADD_OPTION(m_program_settings);
    PA_ADD_OPTION(DELAY_STEP_SIZE);
    PA_ADD_OPTION(SAVE_EVERY);
    PA_ADD_OPTION(MAX_RARE_CANDIES);
    PA_ADD_OPTION(PROFILE);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

std::vector<std::pair<uint64_t, uint16_t>> SeedFarmer::flip_seed_map(const std::map<uint16_t, std::vector<uint64_t>>& seed_to_times){
   std::vector<std::pair<uint64_t, uint16_t>> seed_list = {}; 
   for (auto seed_times : seed_to_times){
        std::vector<uint64_t>& times = seed_times.second;
        uint64_t summed_times = 0;
        for (auto t : times){
            summed_times += t;
        }
        uint64_t avg_time = uint64_t(std::floor(summed_times / times.size()));
        std::pair<uint64_t, uint16_t> pair = { avg_time, seed_times.first };
        seed_list.emplace_back(pair);
   }

    std::sort(seed_list.begin(), seed_list.end(), [](auto &left, auto &right) {
        return left.first < right.first;
    });

    return seed_list;
}

JsonValue SeedFarmer::seeds_to_json(
    const GameVersion& game_version, 
    const Language& language, 
    const AudioSetting& audio,
    const SeedButton& seed_button, 
    const BlackoutButton& extra_button, 
    const std::vector<std::pair<uint64_t, uint16_t>>& seed_list
){
    JsonObject json_result;
    std::string version_str = game_version == GameVersion::firered ? "Switch FireRed" : "Switch LeafGreen";
    std::string language_str;
    switch (language){
    case Language::Japanese:
        language_str = "Japanese";
        break;
    case Language::Spanish:
        language_str = "Spanish";
        break;
    case Language::French:
        language_str = "French";
        break;
    case Language::German:
        language_str = "German";
        break;
    case Language::Italian:
        language_str = "Italian";
        break;
    case Language::English:
    default:
        language_str = "English";
        break;
    }
    std::string audio_str = audio == AudioSetting::mono ? "Mono" : "Stereo";
    std::string seed_button_str;
    switch (seed_button){
    case SeedButton::Start:
        seed_button_str = "Start";
        break;
    case SeedButton::L:
        seed_button_str = "L";
        break;
    case SeedButton::A:
    default:
        seed_button_str = "A";
        break;
    }
    std::string extra_button_str;
    switch (extra_button){
    case BlackoutButton::L:
        extra_button_str = "L";
        break;
    case BlackoutButton::R:
        extra_button_str = "R";
        break;
    case BlackoutButton::None:
    default:
        extra_button_str = "None";
        break;
    }

    JsonObject seeds_object;
    for (auto time_seed : seed_list){ 
        seeds_object[std::to_string(time_seed.first)] = to_hex_string(time_seed.second);
    }    
    
    json_result["GameVersion"] = version_str;
    json_result["Audio"] = audio_str;
    json_result["SeedButton"] = seed_button_str;
    json_result["ExtraButton"] = extra_button_str;
    json_result["Seeds"] = std::move(seeds_object);

    return json_result;
}

void SeedFarmer::save_seeds(
    const std::string& output_json_filename,
    const GameVersion& game_version, 
    const Language& language, 
    const AudioSetting& audio,
    const SeedButton& seed_button, 
    const BlackoutButton& extra_button, 
    const std::map<uint16_t, std::vector<uint64_t>>& seed_to_times   
){
    std::vector<std::pair<uint64_t, uint16_t>> seed_list = flip_seed_map(seed_to_times);
    JsonValue json = seeds_to_json(GAME_VERSION, LANGUAGE, AUDIO_SETTING, SEED_BUTTON, EXTRA_BUTTON, seed_list);
    json.dump(output_json_filename);
}


void SeedFarmer::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    /*
    * Settings: Text Speed fast
    */

    SeedFarmer_Descriptor::Stats& stats = env.current_stats<SeedFarmer_Descriptor::Stats>();

    std::string output_json_filename = std::string(FILE_NAME) + ".json";
    QFile file(QString::fromStdString(output_json_filename));
    if (file.open(QFile::ReadOnly)){
        throw FileException(nullptr, PA_CURRENT_FUNCTION, "Given file name already exists. Choose a different file name.", output_json_filename);
    }

    if (MAX_SEED_DELAY < MIN_SEED_DELAY){
        throw UserSetupError(env.console, "The minimum seed delay must be lower than the maximum seed delay.");
    }

    home_black_border_check(env.console, context);

    RNG_FILTERS.reset();
    RNG_CALIBRATION.reset_hits();

    BaseStats BASE_STATS;
    int16_t GENDER_THRESHOLD = -1;
    switch (TARGET){
    case PokemonFRLG_RngTarget::magikarp:
        BASE_STATS = { 20, 10, 55, 15, 20, 80 };
        GENDER_THRESHOLD = 126;
        break;
    case PokemonFRLG_RngTarget::hitmonchan:
        BASE_STATS = { 50, 105, 79, 35, 110, 76 };
        GENDER_THRESHOLD = -1;
        break;
    case PokemonFRLG_RngTarget::hitmonlee:
        BASE_STATS = { 50, 120, 53, 35, 110, 87 };
        GENDER_THRESHOLD = -1;
        break;
    case PokemonFRLG_RngTarget::eevee:
        BASE_STATS = { 55, 55, 50, 45, 65, 55 };
        GENDER_THRESHOLD = 30;
        break;
    case PokemonFRLG_RngTarget::lapras:
        BASE_STATS = { 130, 85, 80, 85, 95, 60 };
        GENDER_THRESHOLD = 126;
        break;
    case PokemonFRLG_RngTarget::omanyte:
        BASE_STATS = { 35, 40, 100, 90, 55, 35 };
        GENDER_THRESHOLD = 30;
        break;
    case PokemonFRLG_RngTarget::kabuto:
        BASE_STATS = { 30, 80, 90, 55, 45, 55 };
        GENDER_THRESHOLD = 30;
        break;
    case PokemonFRLG_RngTarget::aerodactyl:
        BASE_STATS = { 80, 105, 65, 60, 75, 130 };
        GENDER_THRESHOLD = 30;
        break;
    case PokemonFRLG_RngTarget::gamecornerabra:
        BASE_STATS = { 25, 20, 15, 105, 55, 90 };
        GENDER_THRESHOLD = 63;
        break;
    case PokemonFRLG_RngTarget::gamecornerclefairy:
        BASE_STATS = { 70, 45, 48, 60, 65, 35 };
        GENDER_THRESHOLD = 190;
        break;
    case PokemonFRLG_RngTarget::gamecornerdratinifr:
    case PokemonFRLG_RngTarget::gamecornerdratinilg:
        BASE_STATS = { 41, 64, 45, 50, 50, 50 };
        GENDER_THRESHOLD = 126;
        break;
    case PokemonFRLG_RngTarget::gamecornerscyther:
        BASE_STATS = { 70, 110, 80, 55, 80, 105 };
        GENDER_THRESHOLD = 126;
        break;
    case PokemonFRLG_RngTarget::gamecornerpinsir:
        BASE_STATS = { 65, 125, 100, 55, 70, 85 };
        GENDER_THRESHOLD = 126;
        break;
    case PokemonFRLG_RngTarget::gamecornerporygon:
        BASE_STATS = { 65, 60, 70, 85, 75, 40 };
        GENDER_THRESHOLD = -1;
        break;
    case PokemonFRLG_RngTarget::togepi:
        BASE_STATS = { 35, 20, 65, 40, 65, 20 };
        GENDER_THRESHOLD = 30;
        break; 
    default:
        break;
    }

    static const int64_t FIXED_SEED_OFFSET = -845; // milliseconds, approximate
    static const int64_t FIXED_ADVANCES_OFFSET = 160; // frames, approximate

    static const uint64_t CONTINUE_SCREEN_FRAMES = 200;

    static const double SEED_BUMPS[] = {0, 1, -1, 2, -2};

    static const uint8_t MAX_HISTORY_LENGTH = 10;

    const uint64_t ADVANCES_CALIBRATION_RADIUS = 256;

    static const std::set<std::string> SPECIES_LIST = {
        "magikarp", "hitmonchan", "hitmonlee", "eevee", "lapras",
        "omanyte", "kabuto", "aerodactyl", 
        "abra", "clefairy", "dratini", "scyther", "pinsir", "porygon",
        "togepi"
    };

    std::vector<uint16_t> ALL_SEED_VALUES = {};
    for (uint32_t i = 0; i<65536; i++){
        ALL_SEED_VALUES.emplace_back(static_cast<uint16_t>(i));
    }

    env.log("RNG Target: " + std::to_string(TARGET.current_value()));
    env.log("Target Advances: " + std::to_string(ADVANCES));

    AdvRngSearcher searcher(0, ADVANCES, AdvRngMethod::Method1);

    RngCalibrations calibrations = {
        RNG_CALIBRATION.seed_calibration / FRLG_FRAME_DURATION,
        RNG_CALIBRATION.csf_calibration,
        RNG_CALIBRATION.advances_calibration
    };
    env.log("Initial Seed calibration (frames): " + std::to_string(calibrations.seed_offset));
    env.log("Initial CSF calibration (frames): " + std::to_string(calibrations.csf_offset));
    env.log("Initial In-game calibration (frames x2): " + std::to_string(calibrations.ingame_offset));

    Milliseconds launch_delay = INITIAL_LAUNCH_DELAY;

    RngAdvanceHistory advance_history;
    RngCalibrationHistory calibration_history; 

    std::map<uint16_t, std::vector<uint64_t>> seed_to_times = {};

    uint16_t failed_searches = 0;

    // first, calibrate advances
    while (true){
        if (failed_searches >= 5){
            env.log("Failed to find any matches 5 times in a row");
            OperationFailedException::fire(
                ErrorReport::NO_ERROR_REPORT,
                "Failed to find any matches 5 times in a row.",
                env.console
            ); 
            break;
        }

        send_program_status_notification(
            env, NOTIFICATION_STATUS_UPDATE,
            "Calibrating advances."
        );
        env.update_stats();


        uint64_t advances_radius = get_advances_radius(env.console, calibration_history, ADVANCES_CALIBRATION_RADIUS);

        if (calibration_history.results.size() > 0){
            calibrations.ingame_offset = get_advances_calibration_frames(calibration_history, ADVANCES);
        }

        // if previous resets had uncertain advances, slightly modify the seed delay to try to hit a different target
        double seed_bump = SEED_BUMPS[advance_history.results.size() % 5];
        calibrations.seed_offset += seed_bump;

        uint64_t ingame_advances = ADVANCES - CONTINUE_SCREEN_FRAMES;

        RngTimings timings = prepare_timings(
            env.console, TARGET,
            MIN_SEED_DELAY, CONTINUE_SCREEN_FRAMES, ingame_advances,
            false, calibrations,
            FIXED_SEED_OFFSET, FIXED_ADVANCES_OFFSET
        );

        env.log("Resetting Game...");
        reset_and_perform_blind_sequence(
            env.console, context, TARGET, 
            SEED_BUTTON, EXTRA_BUTTON, timings, 
            launch_delay, false, PROFILE
        );
        stats.resets++; 

        RNG_FILTERS.reset();
        RNG_CALIBRATION.set_calibrations(calibrations);
        RNG_CALIBRATION.reset_hits();

        check_for_shiny(env.console, context, TARGET);

        AdvObservedPokemon pokemon = read_summary(env.console, context, LANGUAGE, SPECIES_LIST);
        AdvRngFilters filters = observation_to_filters(pokemon, BASE_STATS);
        RNG_FILTERS.set(filters);

        std::vector<AdvRngState> search_hits = get_search_results(env.console, searcher, filters, ALL_SEED_VALUES, ADVANCES, advances_radius, GENDER_THRESHOLD);
        RNG_CALIBRATION.set_hits(search_hits);       
        bool finished = update_history(
            env.console, advance_history, calibration_history, MAX_HISTORY_LENGTH, 
            calibrations, search_hits, 1
        );

        for (uint64_t i=0; i<MAX_RARE_CANDIES; i++){
            if (finished){
                break;
            }

            bool failed = use_rare_candy(env.console, context, LANGUAGE, pokemon, filters, BASE_STATS, AdvRngMethod::Method1, false, i == 0);
            if (failed){
                stats.errors++;
                send_program_recoverable_error_notification(
                    env, NOTIFICATION_ERROR_RECOVERABLE,
                    "Failed to use Rare Candy."
                ); 
            }
            RNG_FILTERS.set(filters);

            search_hits = get_search_results(env.console, searcher, filters, ALL_SEED_VALUES, ADVANCES, advances_radius, GENDER_THRESHOLD);
            RNG_CALIBRATION.set_hits(search_hits);     

            bool force_finish = failed || (i == (MAX_RARE_CANDIES - 1));
            finished = update_history(
                env.console, advance_history, 
                calibration_history, MAX_HISTORY_LENGTH, 
                calibrations, search_hits, 
                1, 2, force_finish
            );
        }

        env.log("RNG search finished.");
        if (search_hits.size() == 0){
            failed_searches++;
        }else{
            failed_searches = 0;
        }

        if (search_hits.size() == 1){
            int64_t diff = search_hits[0].advance - ADVANCES;
            if (std::abs(diff) < 2){
                env.log("Advances calibration finished.");
                seed_to_times[search_hits[0].seed] = { MIN_SEED_DELAY };
                break;
            }
        }

    }

    // farm seeds
    uint64_t current_seed_delay = MIN_SEED_DELAY;

    static const uint64_t FARMING_ADVANCES_RADIUS = 2;

    int current_attempts = 0;
    failed_searches = 0;

    env.log("Starting seed farming...");
    while (true){
        if (current_seed_delay > MAX_SEED_DELAY){
            env.log("Finished seed farming!");
            break;
        }

        if (current_attempts > 10){
            env.log("Failed to determine seed 10 times in a row");
            OperationFailedException::fire(
                ErrorReport::NO_ERROR_REPORT,
                "Failed to determine seed 10 times in a row.",
                env.console
            ); 
            break;
        }

        if (failed_searches >= 5){
            env.log("Failed to find any matches 5 times in a row");
            OperationFailedException::fire(
                ErrorReport::NO_ERROR_REPORT,
                "Failed to find any matches 5 times in a row.",
                env.console
            ); 
            break;
        }

        if (stats.resets % SAVE_EVERY == 0){
            save_seeds(output_json_filename, GAME_VERSION, LANGUAGE, AUDIO_SETTING, SEED_BUTTON, EXTRA_BUTTON, seed_to_times);
        }

        send_program_status_notification(
            env, NOTIFICATION_STATUS_UPDATE,
            "Farming seeds."
        );
        env.update_stats();

        CURRENT_SEED_DELAY.set(std::to_string(current_seed_delay));

        uint64_t ingame_advances = ADVANCES - CONTINUE_SCREEN_FRAMES;

        RngTimings timings = prepare_timings(
            env.console, TARGET,
            current_seed_delay, CONTINUE_SCREEN_FRAMES, ingame_advances,
            false, calibrations,
            FIXED_SEED_OFFSET, FIXED_ADVANCES_OFFSET
        );

        // if previous resets had an uncertain seed, slightly modify the advances to try to hit a different target
        uint64_t  adv_bump = current_attempts * 10;
        timings.ingame_delay += adv_bump * FRLG_FRAME_DURATION;

        uint64_t bumped_advances = ADVANCES + adv_bump;

        env.log("Resetting Game...");
        reset_and_perform_blind_sequence(
            env.console, context, TARGET, 
            SEED_BUTTON, EXTRA_BUTTON, timings, 
            launch_delay, false, PROFILE
        );
        stats.resets++; 

        RNG_FILTERS.reset();
        RNG_CALIBRATION.set_calibrations(calibrations);
        RNG_CALIBRATION.reset_hits();

        check_for_shiny(env.console, context, TARGET);

        AdvObservedPokemon pokemon = read_summary(env.console, context, LANGUAGE, SPECIES_LIST);
        AdvRngFilters filters = observation_to_filters(pokemon, BASE_STATS);
        RNG_FILTERS.set(filters);

        std::vector<AdvRngState> search_hits = get_search_results(env.console, searcher, filters, ALL_SEED_VALUES, bumped_advances, FARMING_ADVANCES_RADIUS, GENDER_THRESHOLD);
        RNG_CALIBRATION.set_hits(search_hits);       
        bool finished = search_hits.size() < 2;

        for (uint64_t i=0; i<MAX_RARE_CANDIES; i++){
            if (finished){
                break;
            }

            bool failed = use_rare_candy(env.console, context, LANGUAGE, pokemon, filters, BASE_STATS, AdvRngMethod::Method1, false, i == 0);
            if (failed){
                stats.errors++;
                send_program_recoverable_error_notification(
                    env, NOTIFICATION_ERROR_RECOVERABLE,
                    "Failed to use Rare Candy."
                ); 
            }
            RNG_FILTERS.set(filters);

            search_hits = get_search_results(env.console, searcher, filters, ALL_SEED_VALUES, bumped_advances, FARMING_ADVANCES_RADIUS, GENDER_THRESHOLD);
            RNG_CALIBRATION.set_hits(search_hits);     

            finished = failed || (i == (MAX_RARE_CANDIES - 1)) || (search_hits.size() < 2);
        }

        env.log("RNG search finished.");
        if (search_hits.size() == 0){
            failed_searches++;
        }else{
            failed_searches = 0;
        }

        if (search_hits.size() == 1){
            env.log("Hit: " + to_hex_string(search_hits[0].seed) + " | " + std::to_string(search_hits[0].advance));
            // check if there is already an entry for the found seed
            uint16_t seed = search_hits[0].seed;
            auto it = seed_to_times.find(seed);
            if (it == seed_to_times.end()){
                seed_to_times[seed] = { current_seed_delay };
            }else{
                seed_to_times[seed].emplace_back(current_seed_delay);
            }
            current_seed_delay += DELAY_STEP_SIZE;
            current_attempts = 0;
            stats.seeds = seed_to_times.size();
            continue;
        }

        current_attempts++;
    }

    save_seeds(output_json_filename, GAME_VERSION, LANGUAGE, AUDIO_SETTING, SEED_BUTTON, EXTRA_BUTTON, seed_to_times);

    if (GO_HOME_WHEN_DONE){
        pbf_press_button(context, BUTTON_HOME, 200ms, 1000ms);
    }
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);

}



}
}
}
