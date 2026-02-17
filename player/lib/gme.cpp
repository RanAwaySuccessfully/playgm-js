#include <emscripten/bind.h>
#include "game-music-emu/gme/gme.h"

using namespace std;

string charToString(const char* charPtr) {
    if (charPtr == nullptr) {
        return "";
    }

    return string(charPtr);
}

class PlayerGME {
    public:
        int track;
        int sample_rate;
        int buffer_size;
        string play_error;

        // gme_info_t
        int length;
        int intro_length;
        int loop_length;
        int play_length;
        int fade_length;
        string game;
        string song;
        string author;
        string copyright;
        string comment;
        string dumper;

        PlayerGME() {
            this->track = -1;
            this->sample_rate = 48000;
            this->buffer_size = 1920;
            this->play_error = "";

            this->length = 0;
            this->intro_length = 0;
            this->loop_length = 0;
            this->play_length = 0;
            this->fade_length = 0;

            this->game = "";
            this->song = "";
            this->author = "";
            this->copyright = "";
            this->comment = "";
            this->dumper = "";
        }

        string File(const emscripten::val& file_js, string etype) {
            const char* err;

            unsigned int length = file_js["length"].as<unsigned int>();
            std::vector<uint8_t> file(length);

            auto memory = emscripten::val::module_property("HEAPU8")["buffer"];
            auto memoryView = file_js["constructor"].new_(
                memory, 
                reinterpret_cast<uintptr_t>(file.data()), 
                length
            );

            memoryView.call<void>("set", file_js);

            if (etype == "m3u") {
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

        string Info() {
            const char* err;

            gme_info_t* infoData;
            err = gme_track_info(this->emulator, &infoData, this->track);
            string err_str = charToString(err);

            if (err_str == "") {
                this->length = infoData->length;
                this->intro_length = infoData->intro_length;
                this->loop_length = infoData->loop_length;
                this->play_length = infoData->play_length;
                this->fade_length = infoData->fade_length;

                this->game = infoData->game;
                this->song = infoData->song;
                this->author = infoData->author;
                this->copyright = infoData->copyright;
                this->comment = infoData->comment;
                this->dumper = infoData->dumper;
            }
            
            gme_free_info(infoData);
            return charToString(err);
        }

        string Ready() {
            const char* err = gme_start_track(this->emulator, this->track);
            return charToString(err);
        }

        int Current() {
            return gme_tell(this->emulator);
        }

        void PlayUntil(int ms) {
            gme_set_fade(this->emulator, ms, 8000);
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
                this->play_error = err_str;
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
        .property("play_error", &PlayerGME::play_error)
        .property("length", &PlayerGME::length)
        .property("intro_length", &PlayerGME::intro_length)
        .property("loop_length", &PlayerGME::loop_length)
        .property("play_length", &PlayerGME::play_length)
        .property("fade_length", &PlayerGME::fade_length)
        .property("game", &PlayerGME::game)
        .property("song", &PlayerGME::song)
        .property("author", &PlayerGME::author)
        .property("copyright", &PlayerGME::copyright)
        .property("comment", &PlayerGME::comment)
        .property("dumper", &PlayerGME::dumper)
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