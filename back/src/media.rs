use image::{codecs::gif::GifDecoder, AnimationDecoder, DynamicImage, ImageFormat, RgbImage};
use std::io::{Cursor, Read, Write};

use crate::{
    config::MAX_UNCOMPRESSED_SIZE,
    models::{Animation, Frame, Rgb},
};

pub fn decode_image(bytes: &[u8]) -> Result<Animation, String> {
    if bytes.starts_with(b"PK\x03\x04") {
        return decode_zip(bytes);
    }
    if let Ok(decoder) = GifDecoder::new(Cursor::new(bytes)) {
        if let Ok(frames) = decoder.into_frames().collect_frames() {
            let converted = frames
                .into_iter()
                .map(|f| {
                    let (w, h) = f.buffer().dimensions();
                    Frame {
                        width: w as usize,
                        height: h as usize,
                        pixels: f
                            .buffer()
                            .pixels()
                            .map(|p| Rgb {
                                r: p[0],
                                g: p[1],
                                b: p[2],
                            })
                            .collect(),
                        duration_ms: f.delay().numer_denom_ms().0 / f.delay().numer_denom_ms().1,
                    }
                })
                .collect::<Vec<_>>();
            if !converted.is_empty() {
                return Ok(Animation {
                    is_animated: converted.len() > 1,
                    frames: converted,
                });
            }
        }
    }
    let image = image::load_from_memory(bytes)
        .map_err(|e| e.to_string())?
        .to_rgb8();
    Ok(Animation {
        is_animated: false,
        frames: vec![Frame {
            width: image.width() as usize,
            height: image.height() as usize,
            pixels: image
                .pixels()
                .map(|p| Rgb {
                    r: p[0],
                    g: p[1],
                    b: p[2],
                })
                .collect(),
            duration_ms: 0,
        }],
    })
}

fn decode_zip(bytes: &[u8]) -> Result<Animation, String> {
    let mut archive = zip::ZipArchive::new(Cursor::new(bytes)).map_err(|e| e.to_string())?;
    let mut total = 0u64;
    let mut frames = Vec::new();
    for i in 0..archive.len() {
        let mut file = archive.by_index(i).map_err(|e| e.to_string())?;
        if file.is_dir() || file.size() == 0 || total + file.size() > MAX_UNCOMPRESSED_SIZE {
            continue;
        }
        total += file.size();
        let mut content = Vec::new();
        file.read_to_end(&mut content).map_err(|e| e.to_string())?;
        if let Ok(animation) = decode_image(&content) {
            frames.extend(animation.frames);
        }
    }
    if frames.is_empty() {
        Err("Impossible de décoder le fichier ou le contenu du ZIP.".into())
    } else {
        Ok(Animation {
            is_animated: frames.len() > 1,
            frames,
        })
    }
}

pub fn encode_png(frame: &Frame, serpentine: bool) -> Result<Vec<u8>, String> {
    let mut image = RgbImage::new(frame.width as u32, frame.height as u32);
    for y in 0..frame.height {
        for x in 0..frame.width {
            let sx = if serpentine && y % 2 == 1 {
                frame.width - 1 - x
            } else {
                x
            };
            let p = frame.pixels[y * frame.width + sx];
            image.put_pixel(x as u32, y as u32, image::Rgb([p.r, p.g, p.b]));
        }
    }
    let mut output = Cursor::new(Vec::new());
    DynamicImage::ImageRgb8(image)
        .write_to(&mut output, ImageFormat::Png)
        .map_err(|e| e.to_string())?;
    Ok(output.into_inner())
}

pub fn encode_zip(frames: &[Frame], serpentine: bool) -> Result<Vec<u8>, String> {
    let mut output = Cursor::new(Vec::new());
    let mut zip = zip::ZipWriter::new(&mut output);
    let options = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);
    for (i, frame) in frames.iter().enumerate() {
        zip.start_file(format!("frame_{i:03}.png"), options)
            .map_err(|e| e.to_string())?;
        zip.write_all(&encode_png(frame, serpentine)?)
            .map_err(|e| e.to_string())?;
    }
    zip.finish().map_err(|e| e.to_string())?;
    Ok(output.into_inner())
}
