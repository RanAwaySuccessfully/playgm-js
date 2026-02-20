function PlayGMupload(element) {
    const file = element.files[0];

    file.arrayBuffer().then(data => {
        SMWCentral.SPCPlayer.loadSPC(data, file.name);
    });
}

function returnTrackList(files, extras) {
    let tracks = [];

    const playlistFiles = extras.filter(extra => extra.type === "m3u");
    const playlists = playlistFiles.map(extra => parseM3U(extra));
    const entries = playlists.flat();

    entries.forEach(entry => {
        let file;
        if (files.length > 1) {
            file = files.find(file => {
                return entry._data.filename.startsWith(file.filename);
            });
        } else {
            file = files[0];
        }

        if (file) {
            tracks.push({
                filename: file.filename,
                _m3u_data: entry._data,
                _gme_track: entry._index, // not being used atm...
                date: "", // cant access parseSPC() from here...
                file: file.file,
                extras: [{
                    ...entry._extra
                }]
            });
        }
    });

    const usfs = files.filter(file => file.filename.endsWith(".miniusf"));
    usfs.map(usf => {
        const referenced_extras = [];
        let ref = usf;

        while (ref) {
            ref = findUSFLib(ref, extras);

            if (ref) {
                referenced_extras.push(ref);
            }
        }

        tracks.push({
            ...usf,
            date: "",
            extras: referenced_extras
        });
    });

    if (!entries.length && !tracks.length) {
        tracks = files.map(file => {
            return {
                ...file,
                date: "", // cant access parseSPC() from here...
                extras: [],
            };
        });
    }

    return tracks;
}

function findUSFLib(file, extras) {
    const handle = new DataView(file.file.buffer);
    let offset = handle.getInt32(0x04, true) + 0x10;

    const usfdata = String.fromCharCode(...file.file.slice(offset));
    const match = usfdata.match(/_lib=([\w-]+\.\w+)\x0A/);

    if (match) {
        let lib;

        if (extras.length > 1) {
            lib = extras.find(extra => {
                return extra.filename === match[1];
            });
        } else {
            lib = extras[0];
        }

        if (!lib) {
            /*
            throw new ClientError({
                title: "Missing USFLIB file:",
                description: `**${ match[1] }**`,
                footer: "Usually these files are included in a .zip or folder alongside the rest of the tracks."
            });
            */
            return;
        }

        return lib;
    }
}

function parseM3U(extra) {
    const entries = [];
    let index = 0;

    const buffer = extra.file;
    const input = buffer.toString();
    const lines = input.split(/\r?\n/g);
    lines.forEach(line => {
        if (!line) {
            return;
        }

        if (line.startsWith("#")) {
            return;
        }

        let entry = {
            _extra: extra,
            _index: index,
        };

        const match = line.match(/([^::]+::\w+),(.+)/);
        if (match) {
            const filename = match[1];
            const [ offset, title, length, var5, var6 ] = match[2].split(/,/g);
            
            entry._data = {
                filename,
                offset,
                title,
                length
            };
        } else {
            entry._data = {
                filename: line,
                title: line
            };
        }

        entries.push(entry);
        index++;
    });

    return entries;
}