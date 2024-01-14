

//load the files from the subfolders into an object with keys being the folder names

var fs = require('fs');
var path = require('path');

var files = fs.readdirSync(__dirname + "/data");
var folders = {};
files.forEach(function (file) {
    var stats = fs.statSync(path.join(__dirname + "/data", file));
    if (stats.isDirectory()) {
        // put the full path in the folders object
        folders[file] = fs.readdirSync(path.join(__dirname + "/data", file)).map(p => path.join(__dirname + "/data", file, p));
    }
});

// group folders.random into groups of 3
function chunk(arr, size) {
    var groups = [];
    for (var i = 0; i < arr.length; i += size) {
        groups.push(arr.slice(i, i + size));
    }
    return groups;
}
function shuffle(array) {
    var currentIndex = array.length, temporaryValue, randomIndex;
    // While there remain elements to shuffle...
    while (0 !== currentIndex) { // Pick a remaining element...
        randomIndex = Math.floor(Math.random() * currentIndex);
        currentIndex -= 1; // And swap it with the current element.
        temporaryValue = array[currentIndex];
        array[currentIndex] = array[randomIndex];
        array[randomIndex] = temporaryValue;
    } return array;
}

function zip(...arrays) {
    const length = Math.min(...arrays.map(a => a.length));
    return Array.from({ length }, (_, i) => arrays.map(a => a[i]));
}

function concat32FloatArrs(arr1, arr2) {
    const result = new Float32Array(arr1.length + arr2.length);
    result.set(arr1);
    result.set(arr2, arr1.length);
    return result;
}
function shuffleAugment(arr, n) {
    //n times a shuffled array
    let result = [];
    for (let i = 0; i < n; i++) {
        result = result.concat(shuffle(arr));
    }
    return result;
}
const groups = {
    random:
        zip(
            folders.my.slice(0, 300),
            folders.light.slice(0, 300),
            folders.random.slice(0, 300)
        ).concat(zip(
            folders.random.slice(0, 300),
            shuffle([...folders.on, ...folders.off, ...folders.mood, ...folders.brighter, ...folders.dimmer]).slice(0, 300)
        ))
            .concat(chunk(shuffle(folders.random), 3).slice(0, 100))
            .concat(chunk(shuffle(folders.random), 3).slice(0, 100))
            .concat(chunk(shuffle(folders.random), 3).slice(0, 100)),
    on: zip(...[
        folders.my.slice(0, 300),
        folders.light.slice(0, 300),
        folders.on.slice(0, 300)
    ].map(s => shuffleAugment(s, 3))),
    off: zip(...[
        folders.my.slice(0, 300),
        folders.light.slice(0, 300),
        folders.off.slice(0, 300)
    ].map(s => shuffleAugment(s, 3))),
    mood: zip(...[
        folders.my.slice(0, 300),
        folders.light.slice(0, 300),
        folders.mood.slice(0, 300)
    ].map(s => shuffleAugment(s, 3))),
    brighter: zip(...[
        folders.my.slice(0, 300),
        folders.light.slice(0, 300),
        folders.brighter.slice(0, 300)
    ].map(s => shuffleAugment(s, 3))),
    dimmer: zip(...[
        folders.my.slice(0, 300),
        folders.light.slice(0, 300),
        folders.dimmer.slice(0, 300)
    ].map(s => shuffleAugment(s, 3))),
}

//if the folder doesnt exist create it
if (!fs.existsSync(`${__dirname}/joinedData`)) {
    fs.mkdirSync(`${__dirname}/joinedData`);
}
let i = 4 * 900
for (const [groupName, wavGroup] of Object.entries(groups)) {
    console.log(groupName)
    let n = 0;
    for (const wavPaths of wavGroup) {
        //status each 100
        if (n++ % 100 == 0) console.log(n - 1)

        try {
            const joined = joinWavs(wavPaths);
            //if the folder doesnt exist create it
            if (!fs.existsSync(`${__dirname}/joinedData/${groupName}`)) {
                fs.mkdirSync(`${__dirname}/joinedData/${groupName}`);
            }
            fs.writeFileSync(`${__dirname}/joinedData/${groupName}/${i++}.wav`, joined);
        } catch (e) {
            console.log(wavPaths)
            throw e;
        }
    }
}

function joinWavs(wavGroup) {
    const wav = require('node-wav');
    const wavFiles = wavGroup.map(wavPath => wav.decode(fs.readFileSync(wavPath)));
    const joined = wavFiles.reduce((acc, wav, i, arr) => {
        if (acc.sampleRate !== wav.sampleRate) {
            throw new Error("sample rate mismatch");
        }
        //normalize the volume to -1 to 1
        const max = Math.max(...wav.channelData[0]);
        const min = Math.min(...wav.channelData[0]);
        const maxAbs = Math.max(Math.abs(max), Math.abs(min));
        wav.channelData[0] = wav.channelData[0].map(x => x / maxAbs);

        //if it isnt the first file remove zeros from start of the wav
        if (i !== 0) {
            const firstNonZeroIndex = wav.channelData[0].findIndex(x => x > 0.01);
            wav.channelData[0] = wav.channelData[0].slice(firstNonZeroIndex);
        }
        //if it isnt the last file remove zeros from end of the wav
        if (i !== arr.length - 1) {
            let lastNonZeroIndex = wav.channelData[0].findLastIndex(x => x > 0.01);
            if (i == 0) lastNonZeroIndex -= 2500 // if were at the first file shove it back another few samples to adjust for the "mi light" => "milight"
            wav.channelData[0] = wav.channelData[0].slice(0, lastNonZeroIndex);
        }
        return {
            sampleRate: acc.sampleRate,
            channelData: [concat32FloatArrs(acc.channelData[0], wav.channelData[0])]
        }
    }, {
        sampleRate: wavFiles[0].sampleRate,
        channelData: [new Float32Array(0)]
    });
    return wav.encode(joined.channelData, { sampleRate: joined.sampleRate, float: false, bitDepth: 16 });
}

