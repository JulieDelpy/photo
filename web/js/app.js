// Photo Processing Web UI - app.js
// Backend: C++ photo_web_server on port 9876

let currentImage = null;
let currentImageDataUrl = null;
let apiBase = 'http://localhost:9876';

// ========================
// Chinese display names for checkers & categories
// ========================
const CHECKER_CN = {
    face_detect_check:        '人脸检测',
    face_count_check:         '人脸数量',
    face_confidence_check:    '人脸置信度',
    face_size_check:          '人脸尺寸',
    face_integrity_check:     '人脸完整性',
    eye_closure_check:        '眼睛闭合检测',
    mouth_open_check:         '张嘴检测',
    head_pose_check:          '头部姿态',
    blur_check:               '面部清晰度',
    exposure_check:           '面部曝光',
    contrast_check:           '面部对比度',
    shadow_check:             '面部阴影',
    noise_check:              '面部噪点',
    resolution_check:         '人脸分辨率',
    overexposure_check:       '过曝检测',
    underexposure_check:      '欠曝检测',
    sharpness_check:          '整体清晰度',
    jpeg_artifact_check:      'JPEG 伪影',
    image_blur_check:         '图像模糊',
    image_exposure_check:     '图像曝光',
    image_contrast_check:     '图像对比度',
    image_noise_check:        '图像噪点',
    centering_check:          '人脸居中',
    head_margin_check:        '头顶边距',
    chin_margin_check:        '下巴边距',
    side_margin_check:        '两侧边距',
    face_skin_tone_check:     '面部肤色',
    face_occlusion_check:     '面部遮挡',
    face_aspect_ratio_check:  '面部宽高比',
    background_color_check:   '背景颜色',
    background_uniformity_check: '背景均匀度',
    background_texture_check: '背景纹理',
    background_edge_check:    '背景边缘',
    border_check:             '边缘阴影',
    file_dimension_check:     '图像尺寸',
    file_aspect_ratio_check:  '图像宽高比',
    file_size_check:          '文件大小',
    file_format_check:        '文件格式',
    color_space_check:        '色彩空间',
    dpi_check:                'DPI 检测',
};

const CATEGORY_CN = {
    face:        '人脸',
    state:       '表情姿态',
    quality:     '画质',
    composition: '构图',
    background:  '背景',
    file:        '文件规格',
};

// ========================
// File Upload
// ========================
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');

dropZone.addEventListener('click', () => fileInput.click());

dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('dragover');
});

dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('dragover');
});

dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    const file = e.dataTransfer.files[0];
    if (file) handleFile(file);
});

fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) handleFile(file);
});

function handleFile(file) {
    if (!file.type.startsWith('image/')) {
        alert('请选择图片文件');
        return;
    }

    const reader = new FileReader();
    reader.onload = (e) => {
        currentImageDataUrl = e.target.result;
        currentImage = file;
        showPreview();
    };
    reader.readAsDataURL(file);
}

function showPreview() {
    document.getElementById('previewImage').src = currentImageDataUrl;
    document.getElementById('previewMeta').innerHTML =
        `<div class="tag-row">
            <span class="tag">${currentImage.name}</span>
            <span class="tag">${(currentImage.size / 1024).toFixed(1)} KB</span>
        </div>`;
    document.getElementById('previewCard').style.display = 'block';
    document.getElementById('controls').style.display = 'flex';
    document.getElementById('resultsCard').style.display = 'none';
    document.getElementById('beautyCard').style.display = 'block';
    document.getElementById('beautyPreview').style.display = 'none';
}

// ========================
// Threshold formatting helper
// ========================
function formatThreshold(minVal, maxVal) {
    const hasMin = minVal != null && minVal !== 0;
    const hasMax = maxVal != null && maxVal !== 0;

    if (hasMin && hasMax) {
        return `${minVal} – ${maxVal}`;
    } else if (hasMin) {
        return `≥ ${minVal}`;
    } else if (hasMax) {
        return `≤ ${maxVal}`;
    }
    return '—';
}

function formatActual(val) {
    if (val == null) return '—';
    if (Number.isInteger(val)) return String(val);
    // 0–1 range: use 3 decimal places; larger: 1 decimal
    if (Math.abs(val) < 10) return val.toFixed(3);
    return val.toFixed(1);
}

// ========================
// Quality Check
// ========================
function runQualityCheck() {
    if (!currentImage) {
        alert('请先选择照片');
        return;
    }

    showLoading(true);

    checkImageViaAPI(currentImage)
        .then(report => {
            displayResults(report);
            showLoading(false);
        })
        .catch(err => {
            console.warn('Backend unavailable, using demo data:', err.message);
            setTimeout(() => {
                const report = generateDemoReport();
                displayResults(report);
                showLoading(false);
            }, 600);
        });
}

async function checkImageViaAPI(file) {
    const formData = new FormData();
    formData.append('image', file);
    formData.append('photo_type', document.getElementById('photoType').value);

    const resp = await fetch(`${apiBase}/api/v1/check`, {
        method: 'POST',
        body: formData,
        signal: AbortSignal.timeout(30000)
    });

    if (!resp.ok) throw new Error(`API ${resp.status}`);
    return resp.json();
}

// Demo report when backend is not running — mirrors real C++ output structure
function generateDemoReport() {
    return {
        photo_type: 'id_card_cn',
        photo_display_name: '第二代居民身份证照片 (GA标准) — 演示数据',
        overall_pass: true,
        total_checks: 40,
        passed_checks: 40,
        failed_checks: 0,
        warning_checks: 0,
        face_info: {
            detected: true, confidence: 0.94,
            ear_left: 0.34, ear_right: 0.32, mar: 0.04,
            yaw: -0.8, pitch: 0.4, roll: -2.1
        },
        results: [
            { checker_name:'face_detect_check',        category:'face',        passed:true, severity:'pass', actual_value:1,      min_threshold:0,   max_threshold:0,    message:'检测到人脸' },
            { checker_name:'face_count_check',         category:'face',        passed:true, severity:'pass', actual_value:1,      min_threshold:1,   max_threshold:1,    message:'人脸数量: 1' },
            { checker_name:'face_confidence_check',    category:'face',        passed:true, severity:'pass', actual_value:0.94,   min_threshold:0.8, max_threshold:0,    message:'置信度: 0.94' },
            { checker_name:'face_size_check',          category:'face',        passed:true, severity:'pass', actual_value:0.30,   min_threshold:0.25,max_threshold:0.45, message:'人脸尺寸正常: 面积比=30% 高度比=52%' },
            { checker_name:'face_integrity_check',     category:'face',        passed:true, severity:'pass', actual_value:0,      min_threshold:0,   max_threshold:0,    message:'人脸完整在画面内' },
            { checker_name:'eye_closure_check',        category:'state',       passed:true, severity:'pass', actual_value:0.34,   min_threshold:0.20,max_threshold:0,    message:'眼睛睁开: EAR=0.34' },
            { checker_name:'mouth_open_check',         category:'state',       passed:true, severity:'pass', actual_value:0.04,   min_threshold:0,   max_threshold:0.25, message:'嘴巴闭合: MAR=0.04' },
            { checker_name:'head_pose_check',          category:'state',       passed:true, severity:'pass', actual_value:2.1,    min_threshold:0,   max_threshold:10.0, message:'头部姿态正常' },
            { checker_name:'centering_check',          category:'composition',  passed:true, severity:'pass', actual_value:1.2,    min_threshold:0,   max_threshold:5.0,  message:'人脸居中: 偏移1.2%' },
            { checker_name:'blur_check',               category:'quality',     passed:true, severity:'pass', actual_value:338,    min_threshold:200, max_threshold:0,    message:'面部清晰: Laplacian var=338' },
            { checker_name:'exposure_check',           category:'quality',     passed:true, severity:'pass', actual_value:142,    min_threshold:120, max_threshold:200,  message:'面部曝光适中: 142' },
            { checker_name:'contrast_check',           category:'quality',     passed:true, severity:'pass', actual_value:35,     min_threshold:12,  max_threshold:85,   message:'面部对比度OK: RMS=35' },
            { checker_name:'shadow_check',             category:'quality',     passed:true, severity:'pass', actual_value:0.12,   min_threshold:0,   max_threshold:0.30, message:'面无明显阴影' },
            { checker_name:'noise_check',              category:'quality',     passed:true, severity:'pass', actual_value:3.2,    min_threshold:0,   max_threshold:6.5,  message:'噪点水平可接受' },
            { checker_name:'resolution_check',         category:'quality',     passed:true, severity:'pass', actual_value:18400,  min_threshold:16000,max_threshold:30000,message:'人脸分辨率: 18400px' },
            { checker_name:'sharpness_check',          category:'quality',     passed:true, severity:'pass', actual_value:43.6,   min_threshold:5.0, max_threshold:0,    message:'图像清晰度可接受' },
            { checker_name:'image_blur_check',         category:'quality',     passed:true, severity:'pass', actual_value:1013,   min_threshold:80,  max_threshold:0,    message:'整图清晰度OK' },
            { checker_name:'image_exposure_check',     category:'quality',     passed:true, severity:'pass', actual_value:166,    min_threshold:80,  max_threshold:220,  message:'整图曝光适中' },
            { checker_name:'image_contrast_check',     category:'quality',     passed:true, severity:'pass', actual_value:45,     min_threshold:12,  max_threshold:0,    message:'整图对比度OK' },
            { checker_name:'image_noise_check',        category:'quality',     passed:true, severity:'pass', actual_value:5.3,    min_threshold:0,   max_threshold:9.0,  message:'整图噪点可接受' },
            { checker_name:'jpeg_artifact_check',      category:'quality',     passed:true, severity:'pass', actual_value:1.04,   min_threshold:0,   max_threshold:2.0,  message:'无明显JPEG伪影' },
            { checker_name:'overexposure_check',       category:'quality',     passed:true, severity:'pass', actual_value:0,      min_threshold:0,   max_threshold:0.05, message:'无显著过曝' },
            { checker_name:'underexposure_check',      category:'quality',     passed:true, severity:'pass', actual_value:0,      min_threshold:0,   max_threshold:0.05, message:'无显著欠曝' },
            { checker_name:'head_margin_check',        category:'composition',  passed:true, severity:'pass', actual_value:15,     min_threshold:7,   max_threshold:21,   message:'头顶边距OK: 15px' },
            { checker_name:'chin_margin_check',        category:'composition',  passed:true, severity:'pass', actual_value:20,     min_threshold:7,   max_threshold:0,    message:'下巴边距OK: 20px' },
            { checker_name:'side_margin_check',        category:'composition',  passed:true, severity:'pass', actual_value:25,     min_threshold:10,  max_threshold:0,    message:'两侧边距OK' },
            { checker_name:'face_aspect_ratio_check',  category:'composition',  passed:true, severity:'pass', actual_value:0.85,   min_threshold:0.7, max_threshold:1.0,  message:'面部宽高比OK' },
            { checker_name:'background_color_check',   category:'background',   passed:true, severity:'pass', actual_value:42,     min_threshold:0,   max_threshold:85,   message:'背景颜色OK: S=42 V=195' },
            { checker_name:'background_uniformity_check',category:'background',  passed:true, severity:'pass', actual_value:12,     min_threshold:0,   max_threshold:60,   message:'背景均匀' },
            { checker_name:'background_texture_check', category:'background',   passed:true, severity:'pass', actual_value:8,      min_threshold:0,   max_threshold:22,   message:'背景纹理可接受' },
            { checker_name:'background_edge_check',    category:'background',   passed:true, severity:'pass', actual_value:0.03,   min_threshold:0,   max_threshold:0.12, message:'背景边缘OK' },
            { checker_name:'border_check',             category:'background',   passed:true, severity:'pass', actual_value:0,      min_threshold:0,   max_threshold:22,   message:'背景边缘均匀: 差值=0' },
            { checker_name:'file_dimension_check',     category:'file',         passed:true, severity:'pass', actual_value:358,    min_threshold:358, max_threshold:358,  message:'尺寸: 358×441' },
            { checker_name:'file_aspect_ratio_check',  category:'file',         passed:true, severity:'pass', actual_value:0.811,  min_threshold:0,   max_threshold:0,    message:'宽高比: 0.811' },
            { checker_name:'file_size_check',          category:'file',         passed:true, severity:'pass', actual_value:18,     min_threshold:14,  max_threshold:20,   message:'文件大小: 18KB' },
            { checker_name:'file_format_check',        category:'file',         passed:true, severity:'pass', actual_value:0,      min_threshold:0,   max_threshold:0,    message:'格式: JPEG' },
            { checker_name:'color_space_check',        category:'file',         passed:true, severity:'pass', actual_value:3,      min_threshold:0,   max_threshold:0,    message:'色彩空间: RGB' },
            { checker_name:'dpi_check',                category:'file',         passed:true, severity:'pass', actual_value:350,    min_threshold:350, max_threshold:350,  message:'DPI: 350' },
        ],
        summary: '40/40 通过',
        file_info: { path: '', width: 358, height: 441, file_size_bytes: 18432, format: 'JPEG' }
    };
}

// Core blocking categories: these must ALL pass for an ID photo to be accepted.
// 硬阻断项: 人脸/表情/姿态，只有这 6 个 checker 能返回 FAIL
const HARD_BLOCK_CHECKS = [
    'face_detect_check', 'face_count_check', 'face_confidence_check',
    'eye_closure_check', 'mouth_open_check', 'head_pose_check',
    'head_margin_check', 'centering_check',
    'face_size_check',
    'face_skin_tone_check', 'face_occlusion_check',
    'background_color_check', 'overexposure_check',
    'border_check'
];

// ========================
// 中文消息生成器 — 根据 checker 结构化数据生成专业中文描述
// ========================
function makeMessageCN(checkerName, passed, severity, actual, minVal, maxVal, rawMsg) {
    const cnName = CHECKER_CN[checkerName] || checkerName;

    // 如果 C++ 返回的消息本身就是中文，直接使用
    if (rawMsg && /[一-鿿]/.test(rawMsg)) return rawMsg;

    const av = formatActual(actual);
    const thr = formatThreshold(minVal, maxVal);

    // 根据 checker 类型生成对应中文消息
    switch (checkerName) {
        // ---- 人脸 ----
        case 'face_detect_check':
            return passed ? '成功检测到人脸' : '未检测到人脸';
        case 'face_count_check':
            return passed ? `人脸数量: ${actual}` : `人脸数量异常: 检测到${actual}张人脸（要求1张）`;
        case 'face_confidence_check':
            return passed ? `人脸置信度: ${av}` : `人脸置信度过低: ${av}（要求≥${minVal}）`;
        case 'face_size_check':
            return passed ? `人脸尺寸正常: 面积比=${av}` : rawMsg || `人脸尺寸异常: ${av}（要求${minVal}-${maxVal}）`;
        case 'face_skin_tone_check':
            return passed ? `肤色正常: Cr=${av}` : rawMsg || '面部肤色异常，可能存在偏色';
        case 'face_occlusion_check':
            return passed ? '面部无遮挡' : rawMsg || '面部存在遮挡（头发遮眉/眼）';
        case 'face_integrity_check':
            return passed ? '人脸完整，均在画面边界内' : '人脸超出画面边界';

        // ---- 表情姿态 ----
        case 'eye_closure_check':
            return passed ? `双眼睁开: EAR=${av}` : `疑似闭眼: EAR=${av}（阈值${minVal}）`;
        case 'mouth_open_check':
            return passed ? `嘴部状态正常: MAR=${av}` : rawMsg || `张嘴检测不合格: MAR=${av}`;
        case 'head_pose_check':
            if (passed) return `头部姿态正常: 偏转≤${maxVal}°`;
            return rawMsg || `头部姿态超标: 最大值=${av}°`;

        // ---- 画质 ----
        case 'blur_check':
            return passed ? `面部清晰: Laplacian方差=${av}` : `面部模糊: Laplacian方差=${av}（要求≥${minVal}）`;
        case 'exposure_check':
            return passed ? `面部曝光适中: 亮度=${av}` : `面部曝光异常: 亮度=${av}（要求${minVal}-${maxVal}）`;
        case 'contrast_check':
            return passed ? `面部对比度OK: RMS=${av}` : `面部对比度异常: RMS=${av}（要求${minVal}-${maxVal}）`;
        case 'shadow_check':
            return passed ? '面部光照均匀，无明显阴影' : `面部存在暗部阴影: 阴影比=${av}（阈值${maxVal}）`;
        case 'noise_check':
            return passed ? `面部噪点可接受: 标准差=${av}` : `面部噪点偏高: 标准差=${av}（阈值${maxVal}）`;
        case 'resolution_check':
            return passed ? `人脸分辨率充足: ${av} 像素` : `人脸分辨率不足: ${av} 像素（要求${minVal}-${maxVal}）`;
        case 'overexposure_check':
            return passed ? '无显著过曝区域' : rawMsg || `面部过曝: ${av}（阈值${maxVal}）`;
        case 'underexposure_check':
            return passed ? '无显著欠曝区域' : rawMsg || `面部欠曝: ${av}（阈值${maxVal}）`;
        case 'sharpness_check':
            return passed ? `图像清晰度可接受: Tenengrad=${av}` : `图像清晰度偏低: Tenengrad=${av}（要求≥${minVal}）`;
        case 'image_blur_check':
            return passed ? `整体图像清晰: Laplacian方差=${av}` : `整体图像模糊: Laplacian方差=${av}（要求≥${minVal}）`;
        case 'image_exposure_check':
            return passed ? `整体图像曝光适中: 均值=${av}` : `整体图像曝光异常: 均值=${av}（要求${minVal}-${maxVal}）`;
        case 'image_contrast_check':
            return passed ? `整体图像对比度OK: 标准差=${av}` : `整体图像对比度偏低: 标准差=${av}（要求≥${minVal}）`;
        case 'image_noise_check':
            return passed ? `整体图像噪点可接受: 标准差=${av}` : `整体图像噪点偏高: 标准差=${av}（阈值${maxVal}）`;
        case 'jpeg_artifact_check':
            return passed ? '无明显JPEG压缩伪影' : '检测到JPEG压缩伪影';

        // ---- 构图 ----
        case 'centering_check':
            return passed ? `人脸居中: 偏移${av}%` : `人脸未居中: 偏移${av}%（阈值${maxVal}%）`;
        case 'head_margin_check':
            return passed ? `头顶留白OK: ${av}` : `头顶留白不合适: ${av}（要求${minVal}-${maxVal}）`;
        case 'chin_margin_check':
            return passed ? `下巴留白OK: ${av}px` : `下巴留白不足: ${av}px（要求≥${minVal}px）`;
        case 'side_margin_check':
            return passed ? `两侧留白OK` : `两侧留白不足（要求≥${minVal}px）`;
        case 'face_aspect_ratio_check':
            return passed ? `面部宽高比正常: ${av}` : `面部宽高比异常: ${av}（要求${minVal}-${maxVal}）`;

        // ---- 背景 ----
        case 'background_color_check':
            return passed ? '背景颜色合格' : '背景颜色不符合证件照要求';
        case 'background_uniformity_check':
            return passed ? `背景均匀度OK: 标准差=${av}` : `背景不够均匀: 标准差=${av}（阈值${maxVal}）`;
        case 'background_texture_check':
            return passed ? `背景纹理可接受: 方差=${av}` : `背景纹理过多: 方差=${av}（阈值${maxVal}）`;
        case 'background_edge_check':
            return passed ? `背景边缘干净: 密度=${av}` : `背景存在边缘/杂物: 密度=${av}（阈值${maxVal}）`;
        case 'border_check':
            return passed ? `背景边缘均匀: 差值=${av}` : rawMsg || `背景边缘阴影/不均: ${av}`;

        // ---- 文件规格 ----
        case 'file_dimension_check':
            return passed ? `图像尺寸OK: ${actual}px` : `建议裁剪至 ${maxVal}×${maxVal}`;
        case 'file_aspect_ratio_check':
            return passed ? `宽高比OK` : `宽高比偏离标准，建议裁剪至标准尺寸`;
        case 'file_size_check':
            return passed ? `文件大小OK: ${actual}KB` : `文件大小不合适: ${actual}KB`;
        case 'file_format_check':
            return passed ? '文件格式OK' : '文件格式不兼容，请使用JPEG格式';
        case 'color_space_check':
            return passed ? '色彩空间: RGB' : '请使用RGB色彩空间';
        case 'dpi_check':
            return passed ? `DPI: ${actual}` : `DPI异常: 期望${maxVal} DPI`;

        default:
            return rawMsg || `${cnName}: ${passed ? '正常' : '不合格'}`;
    }
}

function displayResults(report) {
    document.getElementById('resultsCard').style.display = 'block';

    // ---- 两级智能判定 ----
    const checks = report.results || [];
    const hardFails    = checks.filter(c => !c.passed && c.severity === 'fail'
                                   && HARD_BLOCK_CHECKS.includes(c.checker_name));
    const softAdvisory  = checks.filter(c => !c.passed && c.severity === 'warning');
    const effectivePass = report.overall_pass !== false && hardFails.length === 0;

    // ---- 摘要栏 ----
    const summaryBar = document.getElementById('summaryBar');
    if (effectivePass && softAdvisory.length > 0) {
        summaryBar.className = 'summary-bar pass';
        summaryBar.innerHTML = `<strong>合格 (放行)</strong> — `
            + `${report.passed_checks}/${report.total_checks} 核心项通过`
            + `，${softAdvisory.length} 项边缘建议可容忍`;
    } else if (effectivePass) {
        summaryBar.className = 'summary-bar pass';
        summaryBar.innerHTML = `<strong>合格</strong> — `
            + (report.summary || `${report.passed_checks}/${report.total_checks} 通过`);
    } else {
        summaryBar.className = 'summary-bar fail';
        summaryBar.innerHTML = `<strong>不合格</strong> — `
            + `${hardFails.length} 项核心指标未达标`;
    }

    // ---- 预览元信息 ----
    const imgW = (report.file_info || report).width || report.image_width || '?';
    const imgH = (report.file_info || report).height || report.image_height || '?';
    const fi = report.face_info;
    let metaHtml = `<div class="tag-row">
        <span class="tag face">${report.photo_display_name || report.photo_type}</span>
        <span class="tag">${imgW}×${imgH}</span>
        <span class="tag">通过: ${report.passed_checks}</span>`;
    if (report.warning_checks) metaHtml += `<span class="tag">建议: ${report.warning_checks}</span>`;
    if (report.failed_checks) metaHtml += `<span class="tag">拒绝: ${report.failed_checks}</span>`;
    metaHtml += '</div>';

    if (fi && fi.detected) {
        metaHtml += '<div class="tag-row">';
        metaHtml += `<span class="tag" title="左眼开合度">左眼EAR: ${fi.ear_left != null ? fi.ear_left.toFixed(3) : '—'}</span>`;
        metaHtml += `<span class="tag" title="右眼开合度">右眼EAR: ${fi.ear_right != null ? fi.ear_right.toFixed(3) : '—'}</span>`;
        metaHtml += `<span class="tag" title="张嘴程度">MAR: ${fi.mar != null ? fi.mar.toFixed(3) : '—'}</span>`;
        metaHtml += `<span class="tag" title="左右转头">偏转: ${fi.yaw != null ? fi.yaw.toFixed(1)+'°' : '—'}</span>`;
        metaHtml += `<span class="tag" title="上下点头">俯仰: ${fi.pitch != null ? fi.pitch.toFixed(1)+'°' : '—'}</span>`;
        metaHtml += `<span class="tag" title="平面内歪头">倾斜: ${fi.roll != null ? fi.roll.toFixed(1)+'°' : '—'}</span>`;
        metaHtml += '</div>';
    }
    document.getElementById('previewMeta').innerHTML = metaHtml;

    // ---- 统计卡片 ----
    const passCount = checks.filter(c => c.passed).length;
    const warnCount = checks.filter(c => !c.passed && c.severity === 'warning').length;
    const failCount = hardFails.length;
    document.getElementById('statsRow').innerHTML = `
        <div class="stat-card pass-stat">
            <div class="stat-num">${passCount}</div>
            <div class="stat-label">通过</div>
        </div>
        <div class="stat-card warn-stat">
            <div class="stat-num">${warnCount}</div>
            <div class="stat-label">建议优化</div>
        </div>
        <div class="stat-card fail-stat">
            <div class="stat-num">${failCount}</div>
            <div class="stat-label">拒绝</div>
        </div>
    `;

    // ---- 结果表格 ----
    const tbody = document.getElementById('resultsBody');
    tbody.innerHTML = '';

    checks.forEach(r => {
        const cnName  = CHECKER_CN[r.checker_name] || r.checker_name;
        const cnCat   = CATEGORY_CN[r.category] || r.category;
        const passed  = r.passed;
        const isHard  = HARD_BLOCK_CHECKS.includes(r.checker_name);

        // 显示级别：硬阻断FAIL→红色，软WARNING→黄色
        let displaySeverity = r.severity;
        // 边缘项即使 C++ 返回 fail（已全部改为 warning），也按 warning 显示
        if (!passed && !isHard && r.severity === 'fail') {
            displaySeverity = 'warning';
        }

        const tr = document.createElement('tr');
        if (displaySeverity === 'fail')      tr.className = 'fail-row';
        else if (displaySeverity === 'warning') tr.className = 'warn-row';
        else                                 tr.className = 'pass-row';

        // 徽章
        let badgeClass, badgeText;
        if (displaySeverity === 'fail')           { badgeClass = 'fail'; badgeText = '拒绝'; }
        else if (displaySeverity === 'warning')   { badgeClass = 'warn'; badgeText = '建议'; }
        else                                      { badgeClass = 'pass'; badgeText = '通过'; }

        const thr = formatThreshold(r.min_threshold, r.max_threshold);

        // 使用中文消息生成器
        const msg = makeMessageCN(r.checker_name, r.passed, displaySeverity,
                                  r.actual_value, r.min_threshold, r.max_threshold,
                                  r.message);

        tr.innerHTML = `
            <td><span class="badge ${badgeClass}">${badgeText}</span></td>
            <td>${cnName}${isHard && !passed ? ' <span style="color:var(--fail)">🔴</span>' : ''}</td>
            <td>${cnCat}</td>
            <td>${formatActual(r.actual_value)}</td>
            <td>${thr}</td>
            <td>${msg}</td>
        `;
        tbody.appendChild(tr);
    });

    // ---- 筛选栏 ----
    const filterBar = document.getElementById('filterBar');
    filterBar.style.display = 'flex';
    document.getElementById('filterStat').textContent =
        `共 ${checks.length} 项检查`;

    // 重置筛选按钮状态
    filterBar.querySelectorAll('button').forEach(b => b.classList.remove('active'));
    filterBar.querySelector('[data-filter="all"]').classList.add('active');

    // 绑定筛选事件（只绑定一次）
    if (!filterBar._bound) {
        filterBar._bound = true;
        filterBar.addEventListener('click', (e) => {
            const btn = e.target.closest('button[data-filter]');
            if (!btn) return;

            filterBar.querySelectorAll('button').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            const filter = btn.dataset.filter;
            const rows = document.getElementById('resultsBody').querySelectorAll('tr');
            let visible = 0;
            rows.forEach(row => {
                if (filter === 'all') {
                    row.classList.remove('hidden-row');
                    visible++;
                } else {
                    const match = filter === 'pass' ? row.classList.contains('pass-row')
                                : filter === 'warn' ? row.classList.contains('warn-row')
                                : row.classList.contains('fail-row');
                    row.classList.toggle('hidden-row', !match);
                    if (match) visible++;
                }
            });
            document.getElementById('emptyState').style.display = visible === 0 ? 'block' : 'none';
            document.getElementById('filterStat').textContent =
                `显示 ${visible} / ${checks.length} 项`;
        });
    }
}

// ========================
// Beauty Effects
// ========================
function applyBeauty(effectName) {
    if (!currentImage) {
        alert('请先选择照片');
        return;
    }

    showLoading(true);

    applyBeautyViaAPI(effectName)
        .then(resultUrl => {
            document.getElementById('beautyPreview').style.display = 'block';
            document.getElementById('beforeImg').src = currentImageDataUrl;
            document.getElementById('afterImg').src = resultUrl;
            document.getElementById('afterImg').style.filter = '';
            showLoading(false);
        })
        .catch(err => {
            console.warn('Beauty API unavailable:', err.message);
            // CSS filter as visual fallback hint
            document.getElementById('beautyPreview').style.display = 'block';
            document.getElementById('beforeImg').src = currentImageDataUrl;
            const img = document.getElementById('afterImg');
            img.src = currentImageDataUrl;

            const hintMap = {
                skin_smoothing:   'blur(0.5px) contrast(1.05)',
                skin_whitening:   'brightness(1.15) saturate(0.85)',
                eye_enlargement:  'contrast(1.05)',
                face_slimming:    'contrast(1.02)'
            };
            img.style.filter = hintMap[effectName] || '';
            img.title = '预览（CSS模拟）—— 启动 photo_web_server 后可获得真实美颜效果';

            showLoading(false);
        });
}

async function applyBeautyViaAPI(effectName) {
    const params = {};
    if (effectName === 'skin_smoothing') {
        params.strength = (parseInt(document.getElementById('smooth_strength')?.value) || 30) / 100;
        params.texture  = (parseInt(document.getElementById('smooth_texture')?.value)  || 60) / 100;
    } else if (effectName === 'skin_whitening') {
        params.strength = (parseInt(document.getElementById('whiten_strength')?.value) || 30) / 100;
        params.warmth   = (parseInt(document.getElementById('whiten_warmth')?.value)   || 40) / 100;
    } else if (effectName === 'eye_enlargement') {
        params.factor = (parseInt(document.getElementById('eye_factor')?.value) || 10) / 100;
    } else if (effectName === 'face_slimming') {
        params.jaw_strength   = (parseInt(document.getElementById('jaw_strength')?.value)   || 35) / 100;
        params.cheek_strength = (parseInt(document.getElementById('cheek_strength')?.value) || 20) / 100;
    }

    const formData = new FormData();
    formData.append('image', currentImage);
    formData.append('effect', effectName);
    formData.append('params', JSON.stringify(params));

    const resp = await fetch(`${apiBase}/api/v1/beautify`, {
        method: 'POST',
        body: formData,
        signal: AbortSignal.timeout(30000)
    });

    if (!resp.ok) throw new Error(`API ${resp.status}`);
    const blob = await resp.blob();
    return URL.createObjectURL(blob);
}

// ========================
// Loading Indicator
// ========================
function showLoading(show) {
    document.getElementById('loading').style.display = show ? 'block' : 'none';
}

// ========================
// Initialize
// ========================
console.log('Photo Processing Web UI ready');
console.log('Backend API: ' + apiBase);
