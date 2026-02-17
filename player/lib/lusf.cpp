#include <emscripten/bind.h>
#include "lazyusf2/usf/usf.h"

using namespace std;

class PlayerLUSF {
    public:
        int sample_rate;
        int buffer_size;
        string play_error;

        PlayerLUSF() {
            this->sample_rate = 48000;
            this->buffer_size = 1920;
            this->play_error = "";
        }

        string File(const emscripten::val& file_js, string etype) {

        }

        int Initialize() {

        }

        string Info() {

        }

        string Ready() {

        }

        int Current() {

        }

        void PlayUntil(int ms) {

        }

        string Seek(int ms) {

        }

        vector<short> Play() {

        }

        void End() {

        }

    private:
        void* game_state;
};

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::class_<PlayerLUSF>("PlayerLUSF")
        .constructor<>()
        .function("File", &PlayerLUSF::File)
        .function("Initialize", &PlayerLUSF::Initialize)
        .function("Info", &PlayerLUSF::Info)
        .function("Ready", &PlayerLUSF::Ready)
        .function("Current", &PlayerLUSF::Current)
        .function("PlayUntil", &PlayerLUSF::PlayUntil)
        .function("Seek", &PlayerLUSF::Seek)
        .function("Play", &PlayerLUSF::Play)
        .function("End", &PlayerLUSF::End);

    emscripten::register_vector<uint8_t>("VectorUint8");
    emscripten::register_vector<short>("VectorShort");
}