// encode_movie.swift  --  turn a directory of PNG frames (frame_00000.png, ...) into an H.264 MP4,
// using only macOS's own AVFoundation (no ffmpeg needed).
//
//   swiftc -O encode_movie.swift -o encode_movie
//   ./encode_movie FRAMEDIR out.mp4 [FPS] [MEGABITS PER SECOND]
//
// Copyright Danny Calegari 2026, released under the terms of the GNU GPL.

import Foundation
import AVFoundation
import CoreGraphics
import ImageIO

let args = CommandLine.arguments
guard args.count >= 3 else {
    print("usage: encode_movie FRAMEDIR out.mp4 [FPS] [MBPS]")
    exit(1)
}
let dir = args[1], out = args[2]
let fps = Int32(args.count > 3 ? Int(args[3]) ?? 30 : 30)
let mbps = args.count > 4 ? Double(args[4]) ?? 5 : 5

let files = (try FileManager.default.contentsOfDirectory(atPath: dir)).filter { $0.hasSuffix(".png") }.sorted()
guard !files.isEmpty else { print("no PNG frames in \(dir)"); exit(1) }

func load(_ name: String) -> CGImage {
    let url = URL(fileURLWithPath: dir).appendingPathComponent(name)
    guard let src = CGImageSourceCreateWithURL(url as CFURL, nil),
          let img = CGImageSourceCreateImageAtIndex(src, 0, nil) else {
        print("cannot read \(name)"); exit(1)
    }
    return img
}

let first = load(files[0])
let W = first.width & ~1, H = first.height & ~1          // H.264 wants even dimensions
let outURL = URL(fileURLWithPath: out)
try? FileManager.default.removeItem(at: outURL)
let writer = try AVAssetWriter(outputURL: outURL, fileType: .mp4)
let settings: [String: Any] = [
    AVVideoCodecKey: AVVideoCodecType.h264,
    AVVideoWidthKey: W,
    AVVideoHeightKey: H,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: Int(mbps*1_000_000)],
]
let input = AVAssetWriterInput(mediaType: .video, outputSettings: settings)
input.expectsMediaDataInRealTime = false
let attrs: [String: Any] = [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB,
    kCVPixelBufferWidthKey as String: W,
    kCVPixelBufferHeightKey as String: H,
]
let adaptor = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: attrs)
writer.add(input)
guard writer.startWriting() else { print("cannot start writing: \(String(describing: writer.error))"); exit(1) }
writer.startSession(atSourceTime: .zero)

for (i, f) in files.enumerated() {
    while !input.isReadyForMoreMediaData { usleep(2000) }
    let img = (i == 0) ? first : load(f)
    var pb: CVPixelBuffer?
    guard let pool = adaptor.pixelBufferPool,
          CVPixelBufferPoolCreatePixelBuffer(nil, pool, &pb) == kCVReturnSuccess, let buf = pb else {
        print("cannot allocate a pixel buffer"); exit(1)
    }
    CVPixelBufferLockBaseAddress(buf, [])
    let ctx = CGContext(data: CVPixelBufferGetBaseAddress(buf), width: W, height: H, bitsPerComponent: 8,
                        bytesPerRow: CVPixelBufferGetBytesPerRow(buf), space: CGColorSpaceCreateDeviceRGB(),
                        bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue)!
    ctx.setFillColor(CGColor(red: 1, green: 1, blue: 1, alpha: 1))
    ctx.fill(CGRect(x: 0, y: 0, width: W, height: H))
    ctx.draw(img, in: CGRect(x: 0, y: H - img.height, width: img.width, height: img.height))
    CVPixelBufferUnlockBaseAddress(buf, [])
    if !adaptor.append(buf, withPresentationTime: CMTime(value: CMTimeValue(i), timescale: fps)) {
        print("append failed at frame \(i): \(String(describing: writer.error))"); exit(1)
    }
}
input.markAsFinished()
let done = DispatchSemaphore(value: 0)
writer.finishWriting { done.signal() }
done.wait()
print("wrote \(out): \(files.count) frames, \(W)x\(H), \(fps) fps")
