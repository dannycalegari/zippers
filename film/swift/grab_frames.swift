// grab_frames.swift  --  save the frames of a film at given times as PNGs (to look at them).
//
//   swiftc -O -sdk /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk grab_frames.swift -o grab_frames
//   ./grab_frames FILM.mp4 OUTDIR T1 T2 ...        (times in seconds; files OUTDIR/f_TTT.T.png)
//
// Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

import Foundation
import AVFoundation
import ImageIO
import UniformTypeIdentifiers

let a = CommandLine.arguments
guard a.count >= 4 else { print("usage: grab_frames FILM.mp4 OUTDIR T1 [T2 ...]"); exit(1) }
let gen = AVAssetImageGenerator(asset: AVURLAsset(url: URL(fileURLWithPath: a[1])))
gen.requestedTimeToleranceBefore = .zero
gen.requestedTimeToleranceAfter = .zero
for t in a.dropFirst(3).compactMap({ Double($0) }) {
    var actual = CMTime.zero
    guard let cg = try? gen.copyCGImage(at: CMTime(seconds: t, preferredTimescale: 600), actualTime: &actual) else {
        print("no frame at \(t) s"); continue
    }
    let fn = "\(a[2])/f_\(String(format: "%05.1f", t)).png"
    let dst = CGImageDestinationCreateWithURL(URL(fileURLWithPath: fn) as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(dst, cg, nil)
    CGImageDestinationFinalize(dst)
    print(String(format: "%@  (asked %.3f s, got %.3f s)", fn, t, CMTimeGetSeconds(actual)))
}
