#include <csignal>

#include "core/audio/midi.hpp"

#include "services/audio_manager.hpp"
#include "services/engine_manager.hpp"
#include "services/log_manager.hpp"
#include "services/preset_manager.hpp"
#include "services/state_manager.hpp"
#include "services/ui_manager.hpp"
#include "services/clock_manager.hpp"
#include "services/controller.hpp"
#include "board/controller.hpp"
#include "board/audio_driver.hpp"
#include "board/ui/egl_ui_manager.hpp"

using namespace otto;
using namespace otto::services;

int handle_exception(const char* e);
int handle_exception(std::exception& e);
int handle_exception();

int main(int argc, char* argv[])
{
  int result = 0;
  try {
    Application app {
      [&] { return std::make_unique<LogManager>(argc, argv); },
      StateManager::create_default,
      PresetManager::create_default,
      std::make_unique<RTAudioAudioManager>,
      ClockManager::create_default,
      std::make_unique<EGLUIManager>,
      McuFifoController::make_or_dummy,
      EngineManager::create_default
    };

    Controller::current().register_key_handler(Key::settings, [] (auto) {
      if (Controller::current().is_pressed(Key::shift)) {
        Application::current().exit(Application::ErrorCode::user_exit);
      } else {
        (void) std::system("shutdown -h now");
      }
    });

    // Overwrite the logger signal handlers
    std::signal(SIGABRT, Application::handle_signal);
    std::signal(SIGTERM, Application::handle_signal);
    std::signal(SIGINT, Application::handle_signal);

    app.engine_manager->start();
    app.audio_manager->start();
    app.ui_manager->main_ui_loop();

  } catch (const char* e) {
    result = handle_exception(e);
  } catch (std::exception& e) {
    result = handle_exception(e);
  } catch (...) {
    result = handle_exception();
  }

  LOG_F(INFO, "Exiting");
  return result;
}

int handle_exception(const char* e)
{
  LOGE(e);
  LOGE("Exception thrown, exiting!");
  return 1;
}

int handle_exception(std::exception& e)
{
  LOGE(e.what());
  LOGE("Exception thrown, exiting!");
  return 1;
}

int handle_exception()
{
  LOGE("Unknown exception thrown, exiting!");
  return 1;
}
