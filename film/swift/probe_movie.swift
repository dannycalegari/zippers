// probe_movie.swift  --  print what a film holds: duration, tracks, sizes, frame rate, frame
// count, audio format, and each track's time range and edit segments (to check A/V sync).
// Uses only macOS's AVFoundation.
//
//   swiftc -O -sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk probe_movie.swift -o probe_movie
//   ./probe_movie FILM.mp4
//
// Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

import Foundation
import AVFoundation

guard CommandLine.arguments.count >= 2 else { print("usage: probe_movie FILE"); exit(1) }
let path = CommandLine.arguments[1]
let url = URL(fileURLWithPath: path)
let asset = AVURLAsset(url: url)
let sem = DispatchSemaphore(value: 0)
func s(_ t: CMTime) -> String { String(format: "%.4f s", CMTimeGetSeconds(t)) }

Task {
    do {
        let attrs = try FileManager.default.attributesOfItem(atPath: path)
        let bytes = (attrs[.size] as? NSNumber)?.intValue ?? 0
        let dur = try await asset.load(.duration)
        print(String(format: "file: %@  %.2f MB", path, Double(bytes)/1e6))
        print("duration: \(s(dur))")
        for t in try await asset.load(.tracks) {
            let range = try await t.load(.timeRange)
            let rate = try await t.load(.estimatedDataRate)
            print("track \(t.trackID): \(t.mediaType.rawValue)  range \(s(range.start)) + \(s(range.duration))  \(String(format: "%.0f kbit/s", rate/1000))")
            for seg in try await t.load(.segments) {
                let m = seg.timeMapping
                print("   segment: media \(s(m.source.start)) + \(s(m.source.duration)) -> film \(s(m.target.start)) + \(s(m.target.duration))\(seg.isEmpty ? " (empty)" : "")")
            }
            if t.mediaType == .video {
                let size = try await t.load(.naturalSize)
                let fps = try await t.load(.nominalFrameRate)
                let mfd = try await t.load(.minFrameDuration)
                // count the frames
                let reader = try AVAssetReader(asset: asset)
                let out = AVAssetReaderTrackOutput(track: t, outputSettings: nil)
                reader.add(out); reader.startReading()
                var n = 0; var first = CMTime.invalid; var last = CMTime.invalid
                while let sb = out.copyNextSampleBuffer() {
                    if CMSampleBufferGetNumSamples(sb) > 0 {
                        let pts = CMSampleBufferGetPresentationTimeStamp(sb)
                        if !first.isValid { first = pts }
                        last = pts; n += 1
                    }
                }
                print("   video \(Int(size.width))x\(Int(size.height)), \(fps) fps (min frame duration \(s(mfd))), \(n) frames, first pts \(s(first)), last pts \(s(last))")
            } else if t.mediaType == .audio {
                for d in try await t.load(.formatDescriptions) {
                    if let asbd = CMAudioFormatDescriptionGetStreamBasicDescription(d)?.pointee {
                        let id = asbd.mFormatID
                        let cc = String(bytes: [UInt8((id >> 24) & 255), UInt8((id >> 16) & 255), UInt8((id >> 8) & 255), UInt8(id & 255)], encoding: .ascii) ?? "?"
                        print("   audio \(cc), \(asbd.mSampleRate) Hz, \(asbd.mChannelsPerFrame) channels, \(asbd.mFramesPerPacket) frames/packet")
                    }
                }
                let reader = try AVAssetReader(asset: asset)
                let out = AVAssetReaderTrackOutput(track: t, outputSettings: nil)
                reader.add(out); reader.startReading()
                var first = CMTime.invalid; var end = CMTime.zero; var trimStart = CMTime.zero
                while let sb = out.copyNextSampleBuffer() {
                    let pts = CMSampleBufferGetPresentationTimeStamp(sb)
                    if !first.isValid {
                        first = pts
                        if let a = CMGetAttachment(sb, key: kCMSampleBufferAttachmentKey_TrimDurationAtStart, attachmentModeOut: nil) {
                            trimStart = CMTimeMakeFromDictionary((a as! CFDictionary))
                        }
                    }
                    end = CMTimeAdd(pts, CMSampleBufferGetDuration(sb))
                }
                print("   audio first pts \(s(first)) (trim at start \(s(trimStart))), end \(s(end))")
            }
        }
    } catch { print("error: \(error)") }
    sem.signal()
}
sem.wait()
