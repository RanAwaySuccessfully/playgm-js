/*!
 * SMW Central SPC player
 * Copyright (C) 2021 Telinc1
 *
 * Resampling logic from snes_spc_js by Liam Wilson (cosinusoidally)
 * https://github.com/cosinusoidally/snes_spc_js
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
const AudioContext = window.AudioContext || window.webkitAudioContext;

window.SMWCentral ??= {};
window.SMWCentral.SPCPlayer ??= {};
SMWCentral.SPCPlayer.Backend = {
  status: AudioContext == null || window.Uint8Array == null || window.WebAssembly == null ? -2 : 0,
  playerClass: null,
  context: null,
  gainNode: null,
  sourceNode: null,
  locked: true,
  _interval: null,
  _needsDraining: false,
  wasm: null,

  initialize(wasm) {
    this.wasm = wasm;

    if (this.status !== 0) {
      return;
    }

    this.context = new AudioContext({
      sampleRate: 48000 * 2
    });

    this.gainNode = this.context.createGain();
    this.gainNode.connect(this.context.destination);

    this.status = 1;
  },
  getInfo() {
    this.playerClass.Info();
    
    return {
      "duration": this.playerClass.length / 1000,
      "intro_length": this.playerClass.intro_length / 1000,
      "loop_length": this.playerClass.loop_length / 1000,
      "play_length": this.playerClass.play_length / 1000,
      "fade": this.playerClass.fade_length / 1000,
      "game": this.playerClass.game,
      "title": this.playerClass.song,
      "author": this.playerClass.author,
      "copyright": this.playerClass.copyright,
      "comment": this.playerClass.comment,
      "dumper": this.playerClass.dumper
    };
  },
  loadSPC(spc, time = 0) {
    if (this.status !== 1) {
      throw new TypeError("Cannot play SPC right now");
    }

    this.stopSPC(false);
    this.sourceNode?.disconnect(this.gainNode);

    this.playerClass = new this.wasm.PlayerGME();

    this.playerClass.File(spc, "spc");
    this.playerClass.Initialize();
    this.playerClass.Ready();

    if (time > 0) {
      this.playerClass.Seek(time);
    }

    this.playSPC();

    this.sourceNode = new AudioWorkletNode(this.context, "worklet", {
      outputChannelCount: [2]
    });

    this.startedAt = this.context.currentTime - Math.max(0, time);

    this.sourceNode.connect(this.gainNode);
    this.sourceNode.port.onmessage = (event) => {
      this._needsDraining = event.data;
    };

    this.resume();
  },
  stopSPC(pause = true) {
    if (pause) {
      this.pause();
    }

    if (this.playerClass) {
      this.playerClass.End();
      this.playerClass = null;
    }

    clearInterval(this._interval);
    this._needsDraining = false;
  },
  pause() {
    if (this.context != null) {
      this.context.suspend();
    }
  },
  resume() {
    if (this.context != null) {
      this.context.resume();
    }
  },
  getTime() {
    return this.context == null ? 0 : this.context.currentTime - this.startedAt;
  },
  getVolume() {
    return this.gainNode == null ? 1 : Math.min(Math.max(this.gainNode.gain.value, 0), 1.5);
  },
  setVolume(volume, duration = 0) {
    if (this.gainNode === null) {
      return;
    }

    if (duration <= 0.02) {
      this.gainNode.gain.setValueAtTime(Math.min(Math.max(volume, 0), 1.5), this.context.currentTime);
    } else {
      this.gainNode.gain.cancelScheduledValues(this.context.currentTime);
      this.gainNode.gain.exponentialRampToValueAtTime(
        Math.min(Math.max(volume, 0.01), 1.5),
        this.context.currentTime + duration
      );
    }
  },
  fadeOut(duration) {
    this.gainNode.gain.cancelScheduledValues(this.context.currentTime);
    this.gainNode.gain.setValueAtTime(this.getVolume(), this.context.currentTime);
    this.gainNode.gain.exponentialRampToValueAtTime(0.01, this.context.currentTime + duration);
  },
  playSPC() {

    let stillProcessing = false;

    this._interval = setInterval(() => {

      if (!this._needsDraining && !stillProcessing) {
        stillProcessing = true;
        const frame = this.playerClass.Play();
        
        if (frame.size() === 0) {
          if (this.playerClass.play_error) {
            console.error(this.playerClass.play_error);
            this.stopSPC();
          }
          
          return;
        }

        const array = new Float32Array(frame.size());
        for (let j = 0; j < frame.size(); j++) {
          const value = frame.get(j);
          //array[j] = frame.get(j) / 0x7FFF;
          array[j] = (value >= 0x8000) ? -(0x10000 - value) / 0x8000 : value / 0x7FFF;
          //array[j] = frame.get(j) / 32768.0;
          //array[j] = frame.get(j) / 128;
        }
        
        frame.delete();
        this.sourceNode.port.postMessage(array);
        stillProcessing = false;
      }
      
    }, 5);
    
  },
  unlock() {
    if (this.locked && this.context != null) {
      this.context.resume();
      this.locked = false;
    }
  },
};