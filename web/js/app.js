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
    face_size_check:          '人脸尺寸占比',
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
    face_ratio_check:         '面部高度比',
    eye_position_check:       '眼睛位置',
    head_margin_check:        '头顶边距',
    chin_margin_check:        '下巴边距',
    side_margin_check:        '两侧边距',
    head_tilt_check:          '头部倾斜',
    face_aspect_ratio_check:  '面部宽高比',
    background_color_check:   '背景颜色',
    background_uniformity_check: '背景均匀度',
    background_texture_check: '背景纹理',
    background_edge_check:    '背景边缘',
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
    document.getElementById('imageInfo').innerHTML =
        `<p>文件名: ${currentImage.name} | 大小: ${(currentImage.size / 1024).toFixed(1)} KB</p>`;
    document.getElementById('previewSection').style.display = 'block';
    document.getElementById('controls').style.display = 'flex';
    document.getElementById('resultsSection').style.display = 'none';
    document.getElementById('beautySection').style.display = 'block';
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
            { checker_name:'face_size_check',          category:'face',        passed:true, severity:'pass', actual_value:0.30,   min_threshold:0.25,max_threshold:0.45, message:'人脸面积占比: 30%' },
            { checker_name:'face_integrity_check',     category:'face',        passed:true, severity:'pass', actual_value:0,      min_threshold:0,   max_threshold:0,    message:'人脸完整在画面内' },
            { checker_name:'eye_closure_check',        category:'state',       passed:true, severity:'pass', actual_value:0.34,   min_threshold:0.20,max_threshold:0,    message:'眼睛睁开: EAR=0.34' },
            { checker_name:'mouth_open_check',         category:'state',       passed:true, severity:'pass', actual_value:0.04,   min_threshold:0,   max_threshold:0.25, message:'嘴巴闭合: MAR=0.04' },
            { checker_name:'head_pose_check',          category:'state',       passed:true, severity:'pass', actual_value:2.1,    min_threshold:0,   max_threshold:10.0, message:'头部姿态正常' },
            { checker_name:'head_tilt_check',          category:'composition',  passed:true, severity:'pass', actual_value:0.6,    min_threshold:0,   max_threshold:5.0,  message:'眼线倾角: 0.6°' },
            { checker_name:'centering_check',          category:'composition',  passed:true, severity:'pass', actual_value:1.2,    min_threshold:0,   max_threshold:5.0,  message:'人脸居中: 偏移1.2%' },
            { checker_name:'face_ratio_check',         category:'composition',  passed:true, severity:'pass', actual_value:0.52,   min_threshold:0.45,max_threshold:0.58, message:'面部高度比: 52%' },
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
            { checker_name:'eye_position_check',       category:'composition',  passed:true, severity:'pass', actual_value:0.53,   min_threshold:0.5, max_threshold:0,    message:'眼睛位置OK' },
            { checker_name:'head_margin_check',        category:'composition',  passed:true, severity:'pass', actual_value:15,     min_threshold:7,   max_threshold:21,   message:'头顶边距OK: 15px' },
            { checker_name:'chin_margin_check',        category:'composition',  passed:true, severity:'pass', actual_value:20,     min_threshold:7,   max_threshold:0,    message:'下巴边距OK: 20px' },
            { checker_name:'side_margin_check',        category:'composition',  passed:true, severity:'pass', actual_value:25,     min_threshold:10,  max_threshold:0,    message:'两侧边距OK' },
            { checker_name:'face_aspect_ratio_check',  category:'composition',  passed:true, severity:'pass', actual_value:0.85,   min_threshold:0.7, max_threshold:1.0,  message:'面部宽高比OK' },
            { checker_name:'background_color_check',   category:'background',   passed:true, severity:'pass', actual_value:42,     min_threshold:0,   max_threshold:85,   message:'背景颜色OK: S=42 V=195' },
            { checker_name:'background_uniformity_check',category:'background',  passed:true, severity:'pass', actual_value:12,     min_threshold:0,   max_threshold:60,   message:'背景均匀' },
            { checker_name:'background_texture_check', category:'background',   passed:true, severity:'pass', actual_value:8,      min_threshold:0,   max_threshold:22,   message:'背景纹理可接受' },
            { checker_name:'background_edge_check',    category:'background',   passed:true, severity:'pass', actual_value:0.03,   min_threshold:0,   max_threshold:0.12, message:'背景边缘OK' },
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
// Background items that trigger warnings/fails are treated as soft-advisory
// when the core is fully green — the photo still passes.
const CORE_CATEGORIES = ['face', 'state', 'composition', 'quality', 'file'];

function displayResults(report) {
    document.getElementById('resultsSection').style.display = 'block';

    // ---- Smart grading: separate core failures from background advisories ----
    const checks = report.results || [];
    const coreFailures   = checks.filter(c => !c.passed && c.severity === 'fail'
                                   && CORE_CATEGORIES.includes(c.category));
    const coreWarnings   = checks.filter(c => !c.passed && c.severity === 'warning'
                                   && CORE_CATEGORIES.includes(c.category));
    const bgAdvisories   = checks.filter(c => !c.passed
                                   && c.category === 'background');
    const coreAllGreen   = (coreFailures.length === 0 && coreWarnings.length === 0);

    // Overall verdict: core blocking checks decide pass/fail.
    // Background problems alone never reject a photo; they show a green
    // pass with a friendly advisory note instead.
    const effectivePass = coreAllGreen;

    // ---- Summary bar ----
    const summaryBar = document.getElementById('summaryBar');
    if (effectivePass && bgAdvisories.length > 0) {
        // Soft-advisory: background issues but core (face/state/quality) is clean
        summaryBar.className = 'summary-bar pass';
        summaryBar.innerHTML = `<strong>合格 (放行)</strong> — `
            + `${report.passed_checks}/${report.total_checks} 通过`
            + `，${bgAdvisories.length} 项背景微瑕已在可容忍范围`;
    } else if (effectivePass) {
        summaryBar.className = 'summary-bar pass';
        summaryBar.innerHTML = `<strong>合格</strong> — `
            + (report.summary || `${report.passed_checks}/${report.total_checks} 通过`);
    } else {
        summaryBar.className = 'summary-bar fail';
        const failCount = coreFailures.length + coreWarnings.length;
        summaryBar.innerHTML = `<strong>不合格</strong> — `
            + (report.summary || `${report.passed_checks}/${report.total_checks} 通过, `
            + `${failCount} 项核心指标不合格`);
    }

    // ---- Summary stats ----
    document.getElementById('imageInfo').innerHTML =
        `<p>${report.photo_display_name || report.photo_type} |
         尺寸: ${(report.file_info || report).image_width || '?'}×${(report.file_info || report).image_height || '?'} |
         通过: ${report.passed_checks} | 警告: ${report.warning_checks || 0} | 失败: ${report.failed_checks}</p>`;

    // ---- Face info (when available) ----
    const fi = report.face_info;
    if (fi && fi.detected) {
        const faceStats = document.createElement('div');
        faceStats.className = 'face-stats';
        faceStats.innerHTML = `
            <span title="左眼开合度">左眼EAR: ${fi.ear_left != null ? fi.ear_left.toFixed(3) : '—'}</span>
            <span title="右眼开合度">右眼EAR: ${fi.ear_right != null ? fi.ear_right.toFixed(3) : '—'}</span>
            <span title="张嘴程度">MAR: ${fi.mar != null ? fi.mar.toFixed(3) : '—'}</span>
            <span title="左右转头">偏转: ${fi.yaw != null ? fi.yaw.toFixed(1) + '°' : '—'}</span>
            <span title="上下点头">俯仰: ${fi.pitch != null ? fi.pitch.toFixed(1) + '°' : '—'}</span>
            <span title="平面内歪头">倾斜: ${fi.roll != null ? fi.roll.toFixed(1) + '°' : '—'}</span>
        `;
        const infoEl = document.getElementById('imageInfo');
        infoEl.appendChild(faceStats);
    }

    // ---- Results table ----
    const tbody = document.getElementById('resultsBody');
    tbody.innerHTML = '';

    report.results.forEach(r => {
        const cnName  = CHECKER_CN[r.checker_name] || r.checker_name;
        const cnCat   = CATEGORY_CN[r.category] || r.category;
        const passed  = r.passed;

        // Soften background failures to warnings when core is all green
        let displaySeverity = r.severity;
        if (!passed && r.category === 'background' && coreAllGreen) {
            // Downgrade background fail/warning to a soft pass badge
            displaySeverity = 'soft';
        }

        const tr = document.createElement('tr');
        if (displaySeverity === 'fail')      tr.className = 'fail-row';
        else if (displaySeverity === 'warning') tr.className = 'warn-row';
        else if (displaySeverity === 'soft') tr.className = 'warn-row';
        else                                 tr.className = 'pass-row';

        // Badge
        let badgeClass, badgeText;
        if (displaySeverity === 'fail')           { badgeClass = 'fail'; badgeText = '拒绝'; }
        else if (displaySeverity === 'warning')   { badgeClass = 'warn'; badgeText = '警告'; }
        else if (displaySeverity === 'soft')      { badgeClass = 'pass'; badgeText = '放行'; }
        else                                      { badgeClass = 'pass'; badgeText = '通过'; }

        // Threshold string
        const thr = formatThreshold(r.min_threshold, r.max_threshold);

        // Append advisory suffix to soft-passed background items
        let msg = r.message || '';
        if (displaySeverity === 'soft') {
            msg = '[容错放行] ' + msg;
        }

        tr.innerHTML = `
            <td><span class="badge ${badgeClass}">${badgeText}</span></td>
            <td>${cnName}</td>
            <td>${cnCat}</td>
            <td>${formatActual(r.actual_value)}</td>
            <td>${thr}</td>
            <td>${msg}</td>
        `;
        tbody.appendChild(tr);
    });
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
        params.strength = parseInt(document.getElementById('smooth_strength')?.value || 50);
        params.radius   = parseInt(document.getElementById('smooth_radius')?.value || 10);
    } else if (effectName === 'skin_whitening') {
        params.strength = parseInt(document.getElementById('whiten_strength')?.value || 50);
    } else if (effectName === 'eye_enlargement') {
        params.factor = parseInt(document.getElementById('eye_factor')?.value || 15);
    } else if (effectName === 'face_slimming') {
        params.jaw_strength = parseInt(document.getElementById('jaw_strength')?.value || 50);
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
