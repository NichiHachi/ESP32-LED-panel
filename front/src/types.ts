export type Rgb = { r: number; g: number; b: number }
export type Matrix = { width: number; height: number; pixels: Rgb[] }
export type AnimationFrame = { pngBase64: string; delayMs: number }
export type ProcessedResult = { animated: boolean; blob?: Blob; frames?: AnimationFrame[] }
export type Config = {
  matrix: { width: number; height: number; max_palette_colors: number; enable_serpentine_layout: boolean }
  options: {
    fit_mode: 'LETTERBOX' | 'CROP' | 'STRETCH'
    color_mode: 'QUANTIZED_KMEANS' | 'GRAYSCALE' | 'RGB565'
    background_color: Rgb
    brightness: number; contrast: number; gamma: number; rotation_deg: number
    flip_horizontal: boolean; flip_vertical: boolean
  }
}
