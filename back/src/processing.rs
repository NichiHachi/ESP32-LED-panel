use crate::models::{ColorMode, FitMode, Frame, MatrixConfig, Options, Rgb};

fn sample(raw: &[u8], width: usize, _height: usize, x: usize, y: usize) -> Rgb {
    let index = (y * width + x) * 3;
    Rgb {
        r: raw[index],
        g: raw[index + 1],
        b: raw[index + 2],
    }
}

pub fn downsample(raw: &[u8], sw: usize, sh: usize, tw: usize, th: usize) -> Frame {
    let mut pixels = vec![Rgb::default(); tw * th];
    let cell_w = sw as f32 / tw as f32;
    let cell_h = sh as f32 / th as f32;
    for ty in 0..th {
        for tx in 0..tw {
            if tw >= sw || th >= sh {
                let x = (((tx as f32 + 0.5) * cell_w) as usize).min(sw - 1);
                let y = (((ty as f32 + 0.5) * cell_h) as usize).min(sh - 1);
                pixels[ty * tw + tx] = sample(raw, sw, sh, x, y);
            } else {
                let x0 = (tx as f32 * cell_w) as usize;
                let x1 = ((tx + 1) as f32 * cell_w) as usize;
                let y0 = (ty as f32 * cell_h) as usize;
                let y1 = ((ty + 1) as f32 * cell_h) as usize;
                let mut sums = [0u64; 3];
                let mut count = 0u64;
                for y in y0..y1.min(sh) {
                    for x in x0..x1.min(sw) {
                        let p = sample(raw, sw, sh, x, y);
                        sums[0] += p.r as u64;
                        sums[1] += p.g as u64;
                        sums[2] += p.b as u64;
                        count += 1;
                    }
                }
                if count > 0 {
                    pixels[ty * tw + tx] = Rgb {
                        r: (sums[0] / count) as u8,
                        g: (sums[1] / count) as u8,
                        b: (sums[2] / count) as u8,
                    };
                }
            }
        }
    }
    Frame {
        width: tw,
        height: th,
        pixels,
        duration_ms: 100,
    }
}

pub fn fit(raw: &[u8], sw: usize, sh: usize, matrix: &MatrixConfig, options: &Options) -> Frame {
    let tw = matrix.width as usize;
    let th = matrix.height as usize;
    match options.fit_mode {
        FitMode::Stretch => downsample(raw, sw, sh, tw, th),
        FitMode::Crop => {
            let scale = (tw as f32 / sw as f32).max(th as f32 / sh as f32);
            let scaled = downsample(
                raw,
                sw,
                sh,
                (sw as f32 * scale) as usize,
                (sh as f32 * scale) as usize,
            );
            let x0 = (scaled.width - tw) / 2;
            let y0 = (scaled.height - th) / 2;
            let mut pixels = Vec::with_capacity(tw * th);
            for y in 0..th {
                for x in 0..tw {
                    pixels.push(scaled.pixels[(y + y0) * scaled.width + x + x0]);
                }
            }
            Frame {
                width: tw,
                height: th,
                pixels,
                duration_ms: 100,
            }
        }
        FitMode::Letterbox => {
            let scale = (tw as f32 / sw as f32).min(th as f32 / sh as f32);
            let scaled = downsample(
                raw,
                sw,
                sh,
                (sw as f32 * scale).max(1.0) as usize,
                (sh as f32 * scale).max(1.0) as usize,
            );
            let mut frame = Frame {
                width: tw,
                height: th,
                pixels: vec![options.background_color; tw * th],
                duration_ms: 100,
            };
            let x0 = (tw - scaled.width) / 2;
            let y0 = (th - scaled.height) / 2;
            for y in 0..scaled.height {
                for x in 0..scaled.width {
                    frame.pixels[(y + y0) * tw + x + x0] = scaled.pixels[y * scaled.width + x];
                }
            }
            frame
        }
    }
}

pub fn transform(mut frame: Frame, options: &Options, max_colors: i32) -> Frame {
    for p in &mut frame.pixels {
        let correction = |value: u8| {
            let mut v =
                (value as f32 / 255.0 - 0.5) * options.contrast + 0.5 + options.brightness - 1.0;
            if options.gamma != 1.0 {
                v = v.clamp(0.0, 1.0).powf(options.gamma);
            }
            (v * 255.0).clamp(0.0, 255.0) as u8
        };
        p.r = correction(p.r);
        p.g = correction(p.g);
        p.b = correction(p.b);
    }
    let rotation = options.rotation_deg.rem_euclid(360);
    let (nw, nh) = if rotation == 90 || rotation == 270 {
        (frame.height, frame.width)
    } else {
        (frame.width, frame.height)
    };
    let mut rotated = Frame {
        width: nw,
        height: nh,
        pixels: vec![Rgb::default(); nw * nh],
        duration_ms: frame.duration_ms,
    };
    for y in 0..frame.height {
        for x in 0..frame.width {
            let sx = if options.flip_horizontal {
                frame.width - 1 - x
            } else {
                x
            };
            let sy = if options.flip_vertical {
                frame.height - 1 - y
            } else {
                y
            };
            let (dx, dy) = match rotation {
                90 => (frame.height - 1 - sy, sx),
                180 => (frame.width - 1 - sx, frame.height - 1 - sy),
                270 => (sy, frame.width - 1 - sx),
                _ => (sx, sy),
            };
            rotated.pixels[dy * nw + dx] = frame.pixels[sy * frame.width + sx];
        }
    }
    match options.color_mode {
        ColorMode::Grayscale => {
            for p in &mut rotated.pixels {
                let v = (0.299 * p.r as f32 + 0.587 * p.g as f32 + 0.114 * p.b as f32) as u8;
                *p = Rgb { r: v, g: v, b: v };
            }
        }
        ColorMode::Rgb565 => {
            for p in &mut rotated.pixels {
                p.r = (p.r & 0xf8) | (p.r >> 5);
                p.g = (p.g & 0xfc) | (p.g >> 6);
                p.b = (p.b & 0xf8) | (p.b >> 5);
            }
        }
        ColorMode::QuantizedKmeans => quantize(&mut rotated.pixels, max_colors as usize),
    }
    rotated
}

fn quantize(pixels: &mut [Rgb], k: usize) {
    if pixels.is_empty() || k == 0 {
        return;
    }
    let count = k.min(pixels.len());
    let mut centers: Vec<[f32; 3]> = (0..count)
        .map(|i| {
            let p = pixels[i * pixels.len() / count];
            [p.r as f32, p.g as f32, p.b as f32]
        })
        .collect();
    for _ in 0..8 {
        let mut sums = vec![[0.0; 3]; count];
        let mut sizes = vec![0usize; count];
        for p in pixels.iter() {
            let (best, _) = centers
                .iter()
                .enumerate()
                .map(|(i, c)| {
                    (
                        i,
                        (p.r as f32 - c[0]).powi(2)
                            + (p.g as f32 - c[1]).powi(2)
                            + (p.b as f32 - c[2]).powi(2),
                    )
                })
                .min_by(|a, b| a.1.partial_cmp(&b.1).unwrap())
                .unwrap();
            sums[best][0] += p.r as f32;
            sums[best][1] += p.g as f32;
            sums[best][2] += p.b as f32;
            sizes[best] += 1;
        }
        for i in 0..count {
            if sizes[i] > 0 {
                centers[i] = [
                    sums[i][0] / sizes[i] as f32,
                    sums[i][1] / sizes[i] as f32,
                    sums[i][2] / sizes[i] as f32,
                ];
            }
        }
    }
    for p in pixels {
        let c = centers
            .iter()
            .min_by(|a, b| distance(p, a).partial_cmp(&distance(p, b)).unwrap())
            .unwrap();
        *p = Rgb {
            r: c[0].clamp(0.0, 255.0) as u8,
            g: c[1].clamp(0.0, 255.0) as u8,
            b: c[2].clamp(0.0, 255.0) as u8,
        };
    }
}

fn distance(p: &Rgb, c: &[f32; 3]) -> f32 {
    (p.r as f32 - c[0]).powi(2) + (p.g as f32 - c[1]).powi(2) + (p.b as f32 - c[2]).powi(2)
}
