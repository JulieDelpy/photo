// Photo Processing Web UI - app.js

let currentImage = null;
let currentImageDataUrl = null;

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
// Quality Check (Simulated)
// ========================
function runQualityCheck() {
    if (!currentImage) {
        alert('请先选择照片');
        return;
    }

    showLoading(true);

    // Simulate processing delay
    setTimeout(() => {
        const photoType = document.getElementById('photoType').value;
        const results = generateSimulatedResults(photoType);
        displayResults(results);
        showLoading(false);
    }, 1500);
}

function generateSimulatedResults(photoType) {
    // This simulates what the C++ backend would return.
    // In production, this calls POST /api/v1/check with the image file.
    const checks = [
        { name: 'face_detect_check', category: 'face', passed: true, actual: 1.0, min: 1.0, max: 0, msg: '检测到人脸' },
        { name: 'face_count_check', category: 'face', passed: true, actual: 1.0, min: 1.0, max: 1.0, msg: '人脸数量: 1' },
        { name: 'face_confidence_check', category: 'face', passed: true, actual: 0.92, min: 0.8, max: 0, msg: '置信度: 0.92' },
        { name: 'face_size_check', category: 'face', passed: true, actual: 0.22, min: 0.15, max: 0, msg: '人脸面积占比: 22%' },
        { name: 'face_integrity_check', category: 'face', passed: true, actual: 0, min: 0, max: 0, msg: '人脸完整在画面内' },
        { name: 'eye_closure_check', category: 'state', passed: true, actual: 0.32, min: 0.25, max: 0, msg: '眼睛睁开: EAR=0.32' },
        { name: 'mouth_open_check', category: 'state', passed: true, actual: 0.15, min: 0, max: 0.60, msg: '嘴巴闭合: MAR=0.15' },
        { name: 'head_pose_check', category: 'state', passed: true, actual: 2.1, min: 0, max: 5.0, msg: '头部姿态正常' },
        { name: 'blur_check', category: 'quality', passed: true, actual: 156.0, min: 100.0, max: 0, msg: '面部清晰: var=156' },
        { name: 'exposure_check', category: 'quality', passed: true, actual: 145.0, min: 100, max: 200, msg: '面部曝光适中: 145' },
        { name: 'contrast_check', category: 'quality', passed: true, actual: 42.0, min: 30.0, max: 0, msg: '面部对比度OK: RMS=42' },
        { name: 'shadow_check', category: 'quality', passed: true, actual: 0.11, min: 0, max: 0.3, msg: '面无明显阴影' },
        { name: 'noise_check', category: 'quality', passed: true, actual: 5.2, min: 0, max: 10.0, msg: '噪点水平可接受' },
        { name: 'background_color_check', category: 'background', passed: true, actual: 8.0, min: 0, max: 30, msg: '背景为白色' },
        { name: 'background_uniformity_check', category: 'background', passed: true, actual: 8.5, min: 0, max: 15.0, msg: '背景均匀' },
        { name: 'centering_check', category: 'composition', passed: true, actual: 2.3, min: 0, max: 5.0, msg: '人脸居中: 偏移2.3%' },
        { name: 'file_dimension_check', category: 'file', passed: true, actual: 358, min: 358, max: 358, msg: '尺寸: 358x441 OK' },
        { name: 'color_space_check', category: 'file', passed: true, actual: 3, min: 3, max: 0, msg: '色彩空间: RGB' },
    ];

    // Simulate some failures for demo variety
    if (Math.random() > 0.5) {
        checks[8].passed = false;
        checks[8].actual = 45.0;
        checks[8].msg = '面部模糊: Laplacian var=45 < 100';
    }

    const passCount = checks.filter(c => c.passed).length;
    const overallPass = passCount === checks.length;

    return {
        photo_type: photoType,
        overall_pass: overallPass,
        total_checks: checks.length,
        passed_checks: passCount,
        failed_checks: checks.length - passCount,
        warning_checks: 0,
        results: checks,
        summary: `${passCount}/${checks.length} checks passed`
    };
}

function displayResults(report) {
    document.getElementById('resultsSection').style.display = 'block';

    // Summary bar
    const summaryBar = document.getElementById('summaryBar');
    summaryBar.className = 'summary-bar ' + (report.overall_pass ? 'pass' : 'fail');
    summaryBar.textContent = report.overall_pass
        ? `PASS - ${report.summary}`
        : `FAIL - ${report.summary}`;

    // Results table
    const tbody = document.getElementById('resultsBody');
    tbody.innerHTML = '';

    report.results.forEach(r => {
        const tr = document.createElement('tr');
        if (!r.passed) tr.className = 'fail-row';
        else tr.className = 'pass-row';

        const badgeClass = r.passed ? 'pass' : 'fail';
        const badgeText = r.passed ? 'PASS' : 'FAIL';

        tr.innerHTML = `
            <td><span class="badge ${badgeClass}">${badgeText}</span></td>
            <td>${r.name}</td>
            <td>${r.category}</td>
            <td>${r.actual.toFixed(1)}</td>
            <td>${r.min > 0 || r.max > 0 ? (r.min > 0 ? '>' + r.min : '') + (r.max > 0 ? '<' + r.max : '') : 'N/A'}</td>
            <td>${r.msg}</td>
        `;
        tbody.appendChild(tr);
    });
}

// ========================
// Beauty Effects (Simulated)
// ========================
function applyBeauty(effectName) {
    if (!currentImage) {
        alert('请先选择照片');
        return;
    }

    showLoading(true);

    // Simulate processing
    setTimeout(() => {
        document.getElementById('beautyPreview').style.display = 'block';
        document.getElementById('beforeImg').src = currentImageDataUrl;

        // In production, this calls POST /api/v1/beautify
        // For demo, apply a simple CSS filter to simulate the effect
        const img = document.getElementById('afterImg');
        img.src = currentImageDataUrl;

        // Apply CSS filter as visual approximation
        switch (effectName) {
            case 'skin_smoothing':
                img.style.filter = 'blur(0.5px) contrast(1.05)';
                break;
            case 'skin_whitening':
                img.style.filter = 'brightness(1.15) saturate(0.85)';
                break;
            case 'eye_enlargement':
                img.style.filter = 'contrast(1.05)';
                break;
            case 'face_slimming':
                img.style.filter = 'contrast(1.02)';
                break;
        }

        showLoading(false);
    }, 1000);
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
console.log('Supported modes: Quality Check + Beauty Effects (Skin Smoothing, Whitening, Eye Enlarge, Face Slim)');
