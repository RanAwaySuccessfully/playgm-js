#include <emscripten/bind.h>
#include "game-music-emu/gme/gme.h"

using namespace std;

string charToString(const char* charPtr) {
    if (charPtr == nullptr) {
        return "";
    }

    return string(charPtr);
}

vector<uint8_t> valToVector(const emscripten::val& file_js) {
    unsigned int length = file_js["length"].as<unsigned int>();
    vector<uint8_t> file(length);

    auto memory = emscripten::val::module_property("HEAPU8")["buffer"];
    auto memoryView = file_js["constructor"].new_(
        memory, 
        reinterpret_cast<uintptr_t>(file.data()), 
        length
    );

    memoryView.call<void>("set", file_js);
    return file;
}

class PlayerGME {
    public:
        int track;
        int sample_rate;
        int buffer_size;
        string last_error;

        PlayerGME() {
            this->track = -1;
            this->sample_rate = 48000;
            this->buffer_size = 1920;
            this->last_error = "";
        }

        string File(const emscripten::val& file_js, string file_name) {
            const char* err;
            vector<uint8_t> file = valToVector(file_js);

            if (file_name.ends_with("m3u")) {
                err = gme_load_m3u_data(this->emulator, file.data(), file.size());
            } else {
                err = gme_open_data(file.data(), file.size(), &this->emulator, this->sample_rate);
            }

            return charToString(err);
        }

        int Initialize() {
            if (this->track == -1) {
                int count = gme_track_count(this->emulator);

                if (count > 1) {
                    return count;
                } else {
                    this->track = 0;
                }
            }

            return 0;
        }

        emscripten::val Info() {
            const char* err;

            gme_info_t* infoData;
            err = gme_track_info(this->emulator, &infoData, this->track);
            string err_str = charToString(err);

            if (err_str != "") {
                this->last_error = err_str;
                return emscripten::val::null();
            }

            emscripten::val info_obj = emscripten::val::object();
            info_obj.set("length", infoData->length);
            info_obj.set("intro_length", infoData->intro_length);
            info_obj.set("loop_length", infoData->loop_length);
            info_obj.set("play_length", infoData->play_length);
            info_obj.set("fade_length", infoData->fade_length);

            info_obj.set("game", infoData->game);
            info_obj.set("song", infoData->song);
            info_obj.set("author", infoData->author);
            info_obj.set("copyright", infoData->copyright);
            info_obj.set("comment", infoData->comment);
            info_obj.set("dumper", infoData->dumper);
            
            gme_free_info(infoData);
            return info_obj;
        }

        string Ready() {
            const char* err = gme_start_track(this->emulator, this->track);
            return charToString(err);
        }

        int Current() {
            return gme_tell(this->emulator);
        }

        void PlayUntil(int ms) {
            gme_set_fade(this->emulator, ms, 0);
            //gme_set_fade(this->emulator, ms, 8000);
        }

        string Seek(int ms) {
            const char* err = gme_seek(this->emulator, ms);
            return charToString(err);
        }

        vector<short> Play() {
            const char* err;
            vector<short> audio_buffer;

            int ended = gme_track_ended(this->emulator);
            if (ended != 0) {
                return audio_buffer;
            }

            audio_buffer.resize(this->buffer_size);
            err = gme_play(this->emulator, this->buffer_size, audio_buffer.data());
            string err_str = charToString(err);

            if (err_str != "") {
                this->last_error = err_str;
                return audio_buffer;
            }

            return audio_buffer;
        }

        void End() {
	        gme_delete(this->emulator);
        }

    private:
        Music_Emu* emulator;
};

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::class_<PlayerGME>("PlayerGME")
        .constructor<>()
        .property("track", &PlayerGME::track)
        .property("last_error", &PlayerGME::last_error)
        .function("File", &PlayerGME::File)
        .function("Initialize", &PlayerGME::Initialize)
        .function("Info", &PlayerGME::Info)
        .function("Ready", &PlayerGME::Ready)
        .function("Current", &PlayerGME::Current)
        .function("PlayUntil", &PlayerGME::PlayUntil)
        .function("Seek", &PlayerGME::Seek)
        .function("Play", &PlayerGME::Play)
        .function("End", &PlayerGME::End);

    emscripten::register_vector<uint8_t>("VectorUint8");
    emscripten::register_vector<short>("VectorShort");
}