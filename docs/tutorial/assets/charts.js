// assets/charts.js
(function () {
  var style = getComputedStyle(document.documentElement);
  var accent  = style.getPropertyValue('--accent').trim();
  var accent2 = style.getPropertyValue('--accent2').trim();
  var ink     = style.getPropertyValue('--ink').trim();
  var muted   = style.getPropertyValue('--muted').trim();
  var rule    = style.getPropertyValue('--rule').trim();
  var bg2     = style.getPropertyValue('--bg2').trim();

  // --- Chart: 存储位宽 vs 可录时长 (W25Q64 8MB @ 8kHz) ---
  var chart = echarts.init(document.getElementById('chart-capacity'), null, { renderer: 'svg' });
  chart.setOption({
    animation: false,
    textStyle: { fontFamily: 'Segoe UI, PingFang SC, Microsoft YaHei, sans-serif' },
    tooltip: { appendToBody: true, formatter: '{b}: {c} 秒' },
    grid: { top: 48, left: 70, right: 40, bottom: 40 },
    xAxis: {
      type: 'category',
      data: ['12bit 原样存储\n(16KB/s)', '8bit 截取存储\n(8KB/s, 本教程)', '4bit DPCM 压缩\n(4KB/s)'],
      axisLabel: { color: ink, fontSize: 12, interval: 0 },
      axisLine: { lineStyle: { color: rule } },
      axisTick: { show: false }
    },
    yAxis: {
      type: 'value',
      name: '可录时长 (秒)',
      nameTextStyle: { color: muted },
      axisLabel: { color: muted },
      splitLine: { lineStyle: { color: rule } }
    },
    series: [{
      type: 'bar',
      barWidth: '42%',
      data: [512, 1024, 2048],
      itemStyle: { color: accent, borderRadius: [4, 4, 0, 0] },
      label: { show: true, position: 'top', color: ink, fontWeight: 600, formatter: '{c} s' },
      markLine: {
        symbol: 'none',
        lineStyle: { color: accent2, type: 'dashed', width: 2 },
        label: { formatter: '基本要求 ≥ 10 秒', color: accent2, position: 'insideEndTop' },
        data: [{ yAxis: 10 }]
      }
    }]
  });
  window.addEventListener('resize', function () { chart.resize(); });
})();
