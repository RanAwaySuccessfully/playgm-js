class InputWorklet extends AudioWorkletProcessor {

    #buffer = [];
    #done = false;

    constructor() {
        super();
        this.port.onmessage = (event) => {
            if (event.data === "done") {
                this.#done = true;
            }

            this.#buffer.push(event.data);
        };
    }

    process(inputs, outputs, parameters) {
        const output = outputs[0];

        //console.log(this.#buffer.length);
        if (this.#buffer.length >= 100) {
            this.port.postMessage(true);
        } else {
            this.port.postMessage(false);
        }

        if (this.#buffer.length > 0) {

            const frame = this.#buffer[0];
            const frameLength = (frame.length);

            /*
            const left = output[0];
            const length = Math.min(left.length, frame.length);
            
            let i;

            for (i = 0; i < length; i += 1) {
                left[i] = frame[i];
            }
            */

            const left = output[0];
            const right = output[1];
            const length = Math.min(left.length, right.length, frameLength);
            
            let i;

            /*
            for (i = 0; i < length; i++) {
                left[i] = frame[i*2];
                right[i] = frame[(i*2)+1];
            }
            */

            for (i = 0; i < length; i += 2) {
                left[i] = frame[i];
                right[i] = frame[i+1];
            }

            if (frameLength <= left.length) {
                this.#buffer.shift();
            } else {
                this.#buffer[0] = frame.slice(i);
            }
        } else if (this.#done) {
            return false;
        }

        return true;
    }
}

registerProcessor("worklet", InputWorklet);