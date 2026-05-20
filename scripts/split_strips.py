"""
Auto Photo Strip Splitter
自动识别长条图中的单张照片并裁剪保存

Usage:
    python scripts/split_strips.py <input_dir_or_file> [--output <dir>] [--count 5]
    python scripts/split_strips.py testset/strips/ --output testset/extracted/
    python scripts/split_strips.py strip_001.jpg --count 5

Algorithm:
    1. 垂直投影: 计算每一列的像素均值/方差
    2. 找边界: 定位照片之间的间隙区域
    3. 裁剪: 提取每张独立照片
    4. 自动判断横排/竖排
"""

import cv2
import numpy as np
import argparse
import sys
import os
from pathlib import Path


def imread_unicode(path):
    """OpenCV imread with Unicode path support (Windows compatible)"""
    with open(path, 'rb') as f:
        data = np.frombuffer(f.read(), np.uint8)
    img = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if img is None:
        raise FileNotFoundError(f"Cannot decode image: {path}")
    return img


def compute_vertical_projection(gray):
    """
    计算垂直投影（每列的均值）
    返回 (mean_per_column, std_per_column)
    """
    h, w = gray.shape
    mean_per_col = np.mean(gray, axis=0)  # shape: (w,)
    std_per_col = np.std(gray, axis=0)    # shape: (w,)
    return mean_per_col, std_per_col


def compute_horizontal_projection(gray):
    """计算水平投影（每行的均值）"""
    h, w = gray.shape
    mean_per_row = np.mean(gray, axis=1)  # shape: (h,)
    std_per_row = np.std(gray, axis=1)
    return mean_per_row, std_per_row


def detect_orientation(gray, expected_count=5):
    """
    自动判断是横排还是竖排
    返回: 'horizontal' 或 'vertical'
    """
    h, w = gray.shape

    # 垂直投影的方差大 → 横排（列与列之间差异大）
    v_mean, v_std = compute_vertical_projection(gray)
    h_mean, h_std = compute_horizontal_projection(gray)

    v_variance = np.var(v_mean)
    h_variance = np.var(h_mean)

    # 如果宽度 > 高度且垂直投影方差大 → 横排
    if w > h * 1.5:
        return 'horizontal'
    elif h > w * 1.5:
        return 'vertical'
    elif v_variance > h_variance:
        return 'horizontal'
    else:
        return 'vertical'


def find_gap_based_boundaries(mean_proj, expected_count=5):
    """
    基于间隙检测的照片边界查找。

    原理：
    - 照片之间的白色间隙在垂直投影中表现为高亮度区域（~255）
    - 照片内容区域投影值较低（~150-200）
    - 找到白色间隙的中点作为切割线，确保不裁到照片内容

    返回: [(start, end), ...] 每张照片的起止列坐标
    """
    n = len(mean_proj)

    # 平滑投影
    kernel_size = max(3, n // 300)
    kernel = np.ones(kernel_size) / kernel_size
    smoothed = np.convolve(mean_proj, kernel, mode='same')

    # 间隙阈值：投影均值以上或接近 255 的列
    global_mean = np.mean(smoothed)
    gap_threshold = max(global_mean * 1.08, 235.0)

    # 标记所有"间隙列"（白色背景区域）
    gap_mask = smoothed > gap_threshold

    # 合并连续的间隙列，形成间隙区域
    gap_regions = []
    in_gap = False
    gap_start = 0
    for i in range(len(gap_mask)):
        if gap_mask[i] and not in_gap:
            gap_start = i
            in_gap = True
        elif not gap_mask[i] and in_gap:
            w = i - gap_start
            if w >= 3:  # 至少 3 像素才算间隙
                gap_regions.append((gap_start, i, w))
            in_gap = False
    if in_gap:
        w = len(gap_mask) - gap_start
        if w >= 3:
            gap_regions.append((gap_start, len(gap_mask) - 1, w))

    if not gap_regions:
        print("  [warning] No gaps detected. Using equal split.")
        return equal_split(n, expected_count)

    # 分类：边距间隙 vs 照片间间隙
    # 照片间间隙宽度通常在 20-60 像素，边距通常更宽或等于间隙
    gap_widths = [w for _, _, w in gap_regions]
    median_gap_w = np.median(gap_widths)

    # 找照片间的间隙（宽度接近中位数的间隙）
    # 排除最左和最右的大边距
    valid_gaps = []
    for i, (s, e, w) in enumerate(gap_regions):
        # 跳过左边缘 (x < 图像宽度 3%)
        if s < n * 0.03:
            continue
        # 跳过右边缘 (x > 图像宽度 97%)
        if e > n * 0.97:
            continue
        # 跳过太窄的（噪点）
        if w < 5:
            continue
        # 跳过太宽的（可能是大面积空白，不是照片间隙）
        if w > median_gap_w * 3 and w > 80:
            continue
        valid_gaps.append((s, e, w))

    # 取照片间隙的中点作为分割线
    if valid_gaps:
        split_points = [(s + e) // 2 for s, e, _ in valid_gaps]

        # 如果找到的分割线过多，取最显著的 expected_count-1 个
        if len(split_points) > expected_count - 1:
            # 按间隙宽度排序，取最宽的
            valid_gaps.sort(key=lambda g: g[2], reverse=True)
            split_points = [(g[0] + g[1]) // 2 for g in valid_gaps[:expected_count - 1]]
            split_points.sort()
    else:
        split_points = []

    # 验证分割线数量
    if len(split_points) < expected_count - 1:
        print(f"  [warning] Found {len(split_points)} split lines, expected {expected_count - 1}. Using equal split.")
        return equal_split(n, expected_count)

    if len(split_points) > expected_count - 1:
        # 选间距最均匀的 expected_count-1 个
        best = split_points[:expected_count - 1]
        split_points = best
        split_points.sort()

    # 从分割线构建照片区域
    # 去最左非边缘间隙的开头作为第一张照片的开始
    # 去最右非边缘间隙的结尾作为最后一张照片的结束
    left_edge = 0
    right_edge = n - 1

    # 找左边缘: 第一个内容区域开始的位置
    for s, e, w in gap_regions:
        if s < n * 0.03:
            left_edge = e + 1  # 间隙结束后的第一个像素
            break

    # 找右边缘
    for s, e, w in reversed(gap_regions):
        if e > n * 0.97:
            right_edge = s - 1
            break

    regions = []
    prev_x = left_edge
    for sp in split_points:
        regions.append((prev_x, sp))
        prev_x = sp + 1
    regions.append((prev_x, right_edge))

    # 验证宽度一致性
    widths = [e - s for s, e in regions]
    mean_w = np.mean(widths)
    if len(widths) >= 2 and np.std(widths) / mean_w > 0.25:
        print(f"  [warning] Uneven widths (cv={np.std(widths)/mean_w:.2f}), fallback to equal split.")
        return equal_split(n, expected_count)

    return regions


def equal_split(length, count):
    """等分法：在 [0, length) 范围内平分成 count 份"""
    step = length / count
    regions = []
    for i in range(count):
        start = int(i * step)
        end = int((i + 1) * step) - 1
        regions.append((start, end))
    return regions


def find_local_minima(signal, order=10):
    """纯 numpy 找局部最小值"""
    minima = []
    for i in range(order, len(signal) - order):
        window = signal[i - order:i + order + 1]
        if signal[i] == window.min():
            minima.append(i)
    return np.array(minima)

def find_split_lines(mean_proj, expected_count=5):
    """
    备选方法：通过找投影曲线的局部最小值来确定分割线。
    适用于照片之间有暗色分隔线/边框的情况。
    """
    n = len(mean_proj)
    kernel_size = max(5, n // 80)
    kernel = np.ones(kernel_size) / kernel_size
    smoothed = np.convolve(mean_proj, kernel, mode='same')

    order = max(5, n // (expected_count * 4))
    local_min_idx = find_local_minima(smoothed, order)

    if len(local_min_idx) < expected_count - 1:
        return None

    min_vals = [(smoothed[i], i) for i in local_min_idx]
    min_vals.sort()
    split_positions = sorted([pos for val, pos in min_vals[:expected_count - 1]])

    return split_positions


def split_strip(image_path, output_dir, expected_count=5, filename_prefix=""):
    """
    主函数：读取长条图，自动分割为单张照片，保存。

    返回: 保存的文件路径列表
    """
    img = imread_unicode(image_path)
    if img is None:
        print(f"  [error] Cannot read: {image_path}")
        return []

    h, w = img.shape[:2]
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    print(f"  Image size: {w}x{h}")

    # 判断方向
    orientation = detect_orientation(gray, expected_count)
    print(f"  Orientation: {orientation}")

    if orientation == 'horizontal':
        mean_proj, std_proj = compute_vertical_projection(gray)
    else:
        mean_proj, std_proj = compute_horizontal_projection(gray)
        # 转置处理
        img = cv2.rotate(img, cv2.ROTATE_90_CLOCKWISE)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        mean_proj, std_proj = compute_vertical_projection(gray)

    # 方法1: 间隙检测法
    regions = find_gap_based_boundaries(mean_proj, expected_count)

    # 验证: 如果区域宽度方差过大，说明检测不准确，用等分法
    widths = [end - start for start, end in regions]
    if len(widths) >= 2:
        mean_w = np.mean(widths)
        if np.std(widths) / mean_w > 0.3:  # 宽度差异 > 30%
            print(f"  [warning] Uneven region widths (std/mean={np.std(widths)/mean_w:.2f}), using equal split.")
            regions = equal_split(len(mean_proj), expected_count)

    print(f"  Found {len(regions)} regions:", [(s, e, e-s) for s, e in regions])

    # ==============================================
    # Phase 2: 对每张分割出的照片做四边自动裁切
    # 去除照片自身的白边/边框，只保留实际内容区域
    # ==============================================
    saved = []
    base_name = filename_prefix or Path(image_path).stem
    os.makedirs(output_dir, exist_ok=True)

    for i, (start, end) in enumerate(regions):
        # 提取原始分割（包含白边）
        raw_crop = img[0:h, start:end]

        # 自动检测四边内容边界（去除白色边框）
        #
        # 关键发现：照片的"白底"并非纯白（~227），而长条图的间隙是纯白（~255）。
        # 使用 th=250 可将照片背景（82% 非白）与纯白间隙（~5% 非白）清晰分离。
        #
        # 不对称采样：底部看左侧 40%（避右下角水印），右侧看顶部 40%（避底部文字）
        gray_crop = cv2.cvtColor(raw_crop, cv2.COLOR_BGR2GRAY)
        ch, cw = gray_crop.shape
        WHITE_TH = 250       # 高于此值为"纯白"（仅条图间隙，不含照片背景）
        CONTENT_TH = 50.0    # 行/列中非纯白像素占比 > 此值即视为内容

        content = gray_crop < WHITE_TH  # True = 非纯白像素

        # 顶部边界：中间 60% 列
        col_l_t, col_r_t = int(cw * 0.20), int(cw * 0.80)
        row_pct_top = content[:, col_l_t:col_r_t].mean(axis=1) * 100
        top = 0
        while top < ch and row_pct_top[top] <= CONTENT_TH:
            top += 1

        # 底部边界：左侧 40% 列（避开右下角水印），跳过纯白行找到最后的内容行
        col_l_b, col_r_b = int(cw * 0.08), int(cw * 0.42)
        row_pct_bot = content[:, col_l_b:col_r_b].mean(axis=1) * 100
        bottom = ch - 1
        while bottom >= 0 and row_pct_bot[bottom] <= CONTENT_TH:
            bottom -= 1

        # 左边界：中间 60% 行
        row_t_l, row_b_l = int(ch * 0.20), int(ch * 0.80)
        col_pct_left = content[row_t_l:row_b_l, :].mean(axis=0) * 100
        left = 0
        while left < cw and col_pct_left[left] <= CONTENT_TH:
            left += 1

        # 右边界：顶部 40% 行（避开底部水印/文字）
        row_t_r, row_b_r = int(ch * 0.05), int(ch * 0.40)
        col_pct_right = content[row_t_r:row_b_r, :].mean(axis=0) * 100
        right = cw - 1
        while right >= 0 and col_pct_right[right] <= CONTENT_TH:
            right -= 1

        # 安全检查：不能裁得太小
        if bottom - top < 50 or right - left < 50:
            top, bottom, left, right = 0, ch - 1, 0, cw - 1

        crop = raw_crop[top:bottom+1, left:right+1]
        out_path = os.path.join(output_dir, f"{base_name}_{i+1:02d}.jpg")
        cv2.imwrite(out_path, crop)
        saved.append(out_path)

    return saved


def main():
    parser = argparse.ArgumentParser(
        description='Auto Photo Strip Splitter - 自动识别并裁剪长条图中的单张照片'
    )
    parser.add_argument('input', help='Input image file or directory of images')
    parser.add_argument('--output', '-o', default='./output', help='Output directory')
    parser.add_argument('--count', '-c', type=int, default=5, help='Expected number of photos per strip')
    parser.add_argument('--ext', nargs='+', default=['.jpg', '.jpeg', '.png', '.bmp'],
                        help='Image extensions to process (for directory mode)')

    args = parser.parse_args()
    input_path = Path(args.input)

    if not input_path.exists():
        print(f"Error: Path not found: {args.input}")
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)

    if input_path.is_file():
        files = [str(input_path)]
    else:
        files = [str(f) for f in input_path.iterdir()
                 if f.suffix.lower() in args.ext]

    print(f"Processing {len(files)} file(s)...\n")

    total_saved = 0
    for f in files:
        print(f"Processing: {f}")
        saved = split_strip(f, args.output, args.count)
        total_saved += len(saved)
        for s in saved:
            print(f"  -> {s}")
        print()

    print(f"Done. Extracted {total_saved} photo(s) to: {args.output}")


if __name__ == '__main__':
    main()
