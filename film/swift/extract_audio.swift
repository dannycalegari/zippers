// extract_audio.swift  --  decode a film's soundtrack, on the film's own timeline (edit lists and
// AAC priming applied, as a player would), to a 16-bit PCM WAV.  Used by make_movie.py to measure
// the A/V offset of a finished film against the source recording.
//
//   swiftc -O -sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk extract_audio.swift -o extract_audio
//   ./extract_audio FILM.mp4 OUT.wav [RATE]
//
// Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

import Foundation
import AVFoundation

let a = CommandLine.arguments
guard a.count >= 3 else { print("usage: extract_audio FILM.mp4 OUT.wav [RATE]"); exit(1) }
let rate = a.count > 3 ? Double(a[3]) ?? 44100 : 44100
let asset = AVURLAsset(url: URL(fileURLWithPath: a[1]))
let tracks = asset.tracks(withMediaType: .audio)
guard !tracks.isEmpty else { print("no audio in \(a[1])"); exit(1) }
let reader = try AVAssetReader(asset: asset)
let settings: [String: Any] = [
    AVFormatIDKey: kAudioFormatLinearPCM, AVSampleRateKey: rate, AVNumberOfChannelsKey: 2,
    AVLinearPCMBitDepthKey: 16, AVLinearPCMIsFloatKey: false, AVLinearPCMIsBigEndianKey: false,
    AVLinearPCMIsNonInterleaved: false]
// the mix output renders the asset's timeline: edit lists (and the priming they trim) are honoured
let out = AVAssetReaderAudioMixOutput(audioTracks: tracks, audioSettings: settings)
reader.add(out)
reader.timeRange = CMTimeRange(start: .zero, duration: asset.duration)
guard reader.startReading() else { print("cannot read: \(String(describing: reader.error))"); exit(1) }
var pcm = Data()
var firstPTS: Double? = nil
while let sb = out.copyNextSampleBuffer() {
    if firstPTS == nil { firstPTS = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb)) }
    if let bb = CMSampleBufferGetDataBuffer(sb) {
        let n = CMBlockBufferGetDataLength(bb)
        var chunk = Data(count: n)
        chunk.withUnsafeMutableBytes { p in _ = CMBlockBufferCopyDataBytes(bb, atOffset: 0, dataLength: n, destination: p.baseAddress!) }
        pcm.append(chunk)
    }
}
func le32(_ v: UInt32) -> Data { var x = v.littleEndian; return Data(bytes: &x, count: 4) }
func le16(_ v: UInt16) -> Data { var x = v.littleEndian; return Data(bytes: &x, count: 2) }
var w = Data("RIFF".utf8); w += le32(UInt32(36 + pcm.count)); w += Data("WAVE".utf8)
w += Data("fmt ".utf8); w += le32(16); w += le16(1); w += le16(2); w += le32(UInt32(rate))
w += le32(UInt32(rate) * 4); w += le16(4); w += le16(16)
w += Data("data".utf8); w += le32(UInt32(pcm.count)); w += pcm
try w.write(to: URL(fileURLWithPath: a[2]))
print(String(format: "wrote %@: %.4f s at %.0f Hz, first pts %.4f s", a[2], Double(pcm.count) / 4 / rate, rate, firstPTS ?? -1))
