#include <emscripten/bind.h>
#include "lazyusf2/usf/usf.h"
#include "psflib/psflib.h"
#include <cstring>

using namespace std;

struct psf_obj {
    const uint8_t* data;
    size_t size;
};

void* psf_fopen(void* psfData, const char* path) {
   return fopen(path, "r");
}

size_t psf_fread(void* buffer, size_t size, size_t n, void* filePtr) {
   FILE* file = static_cast<FILE*>(filePtr);
   return fread(buffer, size, n, file);
}

int psf_fseek(void* filePtr, int64_t off, int whence) {
   FILE* file = static_cast<FILE*>(filePtr);
   return fseek(file, off, whence);
}

int psf_fclose(void* filePtr) {
   FILE* file = static_cast<FILE*>(filePtr);
   return fclose(file);
}

long psf_ftell(void* filePtr) {
   FILE* file = static_cast<FILE*>(filePtr);
   return ftell(file);
}

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

class PlayerLUSF {
    public:
        int sample_rate;
        int buffer_size;
        string last_error;

        map<string,string> info_data;
        vector<psf_obj> res_data;

        PlayerLUSF() {
            this->sample_rate = 48000;
            this->buffer_size = 1920;
            this->last_error = "";

            this->game_state = NULL;
        }

        string File(const emscripten::val& file_js, string file_name) {
            vector<uint8_t> file = valToVector(file_js);
            this->files.push_back(file_name);

            FILE *fptr = fopen(file_name.c_str(), "w");
            if (fptr != NULL) {
                fwrite(file.data(), sizeof(uint8_t), file.size(), fptr);
                fclose(fptr);
            }

            if (!file_name.ends_with("usflib")) {
                this->start_path = file_name;
            }

            return "";
        }

        int Initialize() {
            this->fade_start = -1;
            this->length = -1;
            this->seek_to = -1;
            this->frame_count = 0;
            int nested_tags = 0;

            psf_file_callbacks cb = { "/", NULL, &psf_fopen, &psf_fread, &psf_fseek, &psf_fclose, &psf_ftell };
            int ver = psf_load(
                start_path.c_str(), &cb, 0x21,
                load_psf_data_callback, this,
                load_psf_info_callback, this, nested_tags,
                load_psf_status_callback, this
            );

            return ver;
        }

        emscripten::val Info() {
            emscripten::val info_obj = emscripten::val::object();

            for (const auto& pair : this->info_data) {
                info_obj.set(pair.first, pair.second);
            }

            return info_obj;
        }

        string Ready() {
            this->game_state = malloc(usf_get_state_size());
            usf_clear(this->game_state);

            if (this->info_data.contains("_enablecompare")) {
                usf_set_compare(this->game_state, 1);
            }

            if (this->info_data.contains("_enableFIFOfull")) {
                usf_set_fifo_full(this->game_state, 1);
            }

            usf_set_hle_audio(this->game_state, 1);

            for (int i = 0; i < this->res_data.size(); i++) {
                psf_obj res_data = this->res_data[i];
                int state = usf_upload_section(this->game_state, &res_data.data[0], res_data.size);

                if (state == -1) {
                    return "failed to start emulator: invalid data uploaded";
                }
            }

            this->last_error = "";
            return "";
        }

        int Current() {
            int frame_ms = this->get_size_of_frame();
            return this->frame_count * frame_ms;
        }

        void PlayUntil(int ms) {
            if (ms == -1) {
                this->fade_start = -1;
                this->length = -1;
                return;
            }

            int frame_ms = this->get_size_of_frame();
            this->fade_start = (ms / frame_ms);
            this->length = (ms + 8000) / frame_ms;
        }

        string Seek(int ms) {
            int frame_ms = this->get_size_of_frame();

            this->seek_to = ms / frame_ms;
            if (this->seek_to < this->frame_count) {
                usf_restart(this->game_state);
            }

            return "";
        }

        vector<short> Play() {
            const char* err;
            vector<short> audio_buffer;

            if ((this->length >= 0) && (this->frame_count >= this->length)) {
                return audio_buffer;
            }

            int buffer_size = this->buffer_size / 2;
            if ((this->seek_to != -1) && (this->seek_to < this->frame_count)) {
                usf_render_resampled(this->game_state, NULL, buffer_size, this->sample_rate);
                this->frame_count++;
                return this->Play();
            }

            audio_buffer.resize(this->buffer_size);
            err = usf_render_resampled(this->game_state, audio_buffer.data(), buffer_size, this->sample_rate);
            this->frame_count++;

            string err_str = charToString(err);
            if (err_str != "") {
                this->last_error = err_str;
                return audio_buffer;
            }

            /*
            if ((this->fade_start >= 0) && (this->frame_count > this->fade_start)) {
                this->doManualFadeout(audio_buffer);
            }
            */

            return audio_buffer;
        }

        void End() {
            if (this->game_state != NULL) {
                usf_shutdown(this->game_state);
                free(this->game_state);
            }

            for (int i = 0; i < this->files.size(); i++) {
                string file_name = this->files[i];
                remove(file_name.c_str());
            }
        }

        static int load_psf_data_callback(void* ctx, const uint8_t* exe, size_t exe_size, const uint8_t* res, size_t res_size) {
            PlayerLUSF* player = static_cast<PlayerLUSF*>(ctx);
            
            void* res_data = malloc(res_size * sizeof(uint8_t));
            
            psf_obj res_obj;
            res_obj.data = static_cast<const uint8_t*>(res_data);
            res_obj.size = res_size;

            memcpy(res_data, res, res_size);
            player->res_data.push_back(res_obj);

            return 0;
        }

        static int load_psf_info_callback(void* ctx, const char *name, const char *value) {
            PlayerLUSF* player = static_cast<PlayerLUSF*>(ctx);
            player->info_data[name] = value;

            return 0;
        }

        static void load_psf_status_callback(void* ctx, const char *message) {
            PlayerLUSF* player = static_cast<PlayerLUSF*>(ctx);
            player->last_error += message;
        }

    private:
        int seek_to;
        int frame_count;
        int fade_start;
        int length;

        void* game_state;
        string start_path;
        vector<string> files;

        int get_size_of_frame() {
            int buffer_size = this->buffer_size;
            int channels = 2;
            int sample_rate = this->sample_rate;

            int frame_ms = ((buffer_size / channels) * 1000) / sample_rate;
            return frame_ms;
        }

        void doManualFadeout(vector<short>& buffer_data) {
            float total_frames = this->length - this->fade_start;
            float remaining_frames = this->length - this->frame_count;
            float volume = remaining_frames / total_frames;
            float volume_next = (remaining_frames - 1) / total_frames;

            if (volume < 0) {
                volume = 0;
            }

            if (volume_next < 0) {
                volume_next = 0;
            }

            float volume_step = (volume - volume_next) / 960;

            for (int i = 0; i < buffer_data.size(); i++) {
                float real_volume = volume - (volume_step * (i / 2));

                buffer_data[i] = float(buffer_data[i]) * real_volume;
            }
        }
};

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::class_<PlayerLUSF>("PlayerLUSF")
        .constructor<>()
        .property("last_error", &PlayerLUSF::last_error)
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