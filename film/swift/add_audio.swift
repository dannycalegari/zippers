// add_audio.swift  --  put a soundtrack on a film, using only macOS's own AVFoundation.
// The music starts START seconds into the audio file and runs for the length of the film, with a
// short fade-in and a fade-out over the last FADE seconds (0: no fade-out; negative: no fades, and
// the video is copied without re-encoding).
//
//   swiftc -O add_audio.swift -o add_audio
//   ./add_audio FILM.mp4 MUSIC.m4a OUT.mp4 [START] [FADE]
//
// Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

import Foundation
import AVFoundation

let args = CommandLine.arguments
guard args.count >= 4 else {
    print("usage: add_audio FILM.mp4 MUSIC.m4a OUT.mp4 [START seconds] [FADE seconds]")
    exit(1)
}
let start = args.count > 4 ? Double(args[4]) ?? 0 : 0
let fade = args.count > 5 ? Double(args[5]) ?? 3 : 3

let film = AVURLAsset(url: URL(fileURLWithPath: args[1]))
let music = AVURLAsset(url: URL(fileURLWithPath: args[2]))
guard let vtrack = film.tracks(withMediaType: .video).first else { print("no video in \(args[1])"); exit(1) }
guard let atrack = music.tracks(withMediaType: .audio).first else { print("no audio in \(args[2])"); exit(1) }

let comp = AVMutableComposition()
let D = film.duration
let t0 = CMTime(seconds: start, preferredTimescale: 44100)
let avail = CMTimeSubtract(music.duration, t0)
let len = CMTimeCompare(avail, D) < 0 ? avail : D
let cv = comp.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid)!
try cv.insertTimeRange(CMTimeRange(start: .zero, duration: D), of: vtrack, at: .zero)
cv.preferredTransform = vtrack.preferredTransform
let ca = comp.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)!
try ca.insertTimeRange(CMTimeRange(start: t0, duration: len), of: atrack, at: .zero)

// a 0.25 s fade-in (no click when starting mid-track) and the fade-out at the end
let params = AVMutableAudioMixInputParameters(track: ca)
params.setVolumeRamp(fromStartVolume: 0, toEndVolume: 1, timeRange: CMTimeRange(start: .zero, duration: CMTime(seconds: 0.25, preferredTimescale: 600)))
if fade > 0 {
    let fs = CMTimeSubtract(len, CMTime(seconds: fade, preferredTimescale: 600))
    params.setVolumeRamp(fromStartVolume: 1, toEndVolume: 0, timeRange: CMTimeRange(start: fs, duration: CMTime(seconds: fade, preferredTimescale: 600)))
}
let mix = AVMutableAudioMix()
mix.inputParameters = [params]
// FADE < 0: no fades at all, and the video is passed through without re-encoding (use this when the
// fades are already in the audio file)
let passthrough = fade < 0

let outURL = URL(fileURLWithPath: args[3])
try? FileManager.default.removeItem(at: outURL)
guard let ex = AVAssetExportSession(asset: comp, presetName: passthrough ? AVAssetExportPresetPassthrough : AVAssetExportPresetHighestQuality) else {
    print("cannot create the export session"); exit(1)
}
ex.outputURL = outURL
ex.outputFileType = .mp4
if !passthrough { ex.audioMix = mix }
let done = DispatchSemaphore(value: 0)
ex.exportAsynchronously { done.signal() }
done.wait()
if ex.status != .completed { print("export failed: \(String(describing: ex.error))"); exit(1) }
print(String(format: "wrote %@: film %.2f s, music from %.2f s for %.2f s, fade-out %.1f s",
             args[3], CMTimeGetSeconds(D), start, CMTimeGetSeconds(len), fade))
