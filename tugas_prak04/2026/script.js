/**
 * Sales Dashboard 2023
 * =====================
 * Loads data from sales_data.json, populates filter dropdowns,
 * computes KPI metrics, and renders/updates all Chart.js charts
 * dynamically whenever a filter changes — no page refresh needed.
 */

// ─── GLOBAL CHART INSTANCES ──────────────────────────────────────────────────
// Stored so we can destroy and recreate them on filter change
const charts = {};

// ─── COLOUR PALETTE ──────────────────────────────────────────────────────────
// Consistent accent colours used across all charts
const PALETTE = [
  '#6366f1', '#22d3ee', '#f59e0b', '#10b981',
  '#f43f5e', '#a78bfa', '#34d399', '#fb923c',
  '#38bdf8', '#e879f9'
];

const ALPHA_PALETTE = PALETTE.map(c => c + 'cc'); // semi-transparent fill

// ─── UTILITY: format number as currency ──────────────────────────────────────
function formatCurrency(value) {
  return '$' + value.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
}

// ─── UTILITY: sum an array of objects by a key ───────────────────────────────
function sumBy(arr, key) {
  return arr.reduce((acc, row) => acc + (row[key] || 0), 0);
}

// ─── UTILITY: group records and aggregate by a field ─────────────────────────
function groupSum(arr, groupKey, valueKey) {
  const map = {};
  arr.forEach(row => {
    const k = row[groupKey];
    map[k] = (map[k] || 0) + row[valueKey];
  });
  return map;
}

// ─── UTILITY: count occurrences of unique values ─────────────────────────────
function countBy(arr, key) {
  const map = {};
  arr.forEach(row => {
    const k = row[key];
    map[k] = (map[k] || 0) + 1;
  });
  return map;
}

// ─── UTILITY: destroy a chart instance if it exists ──────────────────────────
function destroyChart(id) {
  if (charts[id]) {
    charts[id].destroy();
    delete charts[id];
  }
}

// ─── SHARED CHART OPTIONS ────────────────────────────────────────────────────
// Responsive defaults applied to every chart
const baseOptions = {
  responsive: true,
  maintainAspectRatio: false,
  plugins: {
    legend: {
      labels: { font: { size: 12, family: "'Segoe UI', system-ui, sans-serif" }, color: '#6b7280', boxWidth: 12 }
    },
    tooltip: {
      backgroundColor: '#1f2937',
      titleColor: '#f9fafb',
      bodyColor: '#d1d5db',
      cornerRadius: 8,
      padding: 10
    }
  }
};

// ─── POPULATE FILTER DROPDOWNS ───────────────────────────────────────────────
function populateFilters(data) {
  const selectors = {
    filterRegion:   'Region',
    filterCategory: 'Product_Category',
    filterCustomer: 'Customer_Type',
    filterChannel:  'Sales_Channel'
  };

  Object.entries(selectors).forEach(([selectId, field]) => {
    const select = document.getElementById(selectId);
    // Collect unique sorted values
    const unique = [...new Set(data.map(r => r[field]))].sort();
    unique.forEach(val => {
      const opt = document.createElement('option');
      opt.value = val;
      opt.textContent = val;
      select.appendChild(opt);
    });
  });
}

// ─── APPLY FILTERS ───────────────────────────────────────────────────────────
// Returns a filtered subset of the full dataset based on current dropdown values
function applyFilters(data) {
  const region   = document.getElementById('filterRegion').value;
  const category = document.getElementById('filterCategory').value;
  const customer = document.getElementById('filterCustomer').value;
  const channel  = document.getElementById('filterChannel').value;

  return data.filter(row =>
    (region   === 'all' || row.Region           === region)   &&
    (category === 'all' || row.Product_Category === category) &&
    (customer === 'all' || row.Customer_Type    === customer) &&
    (channel  === 'all' || row.Sales_Channel    === channel)
  );
}

// ─── UPDATE KPI CARDS ────────────────────────────────────────────────────────
function updateKPIs(data) {
  const totalRevenue      = sumBy(data, 'Sales_Amount');
  const totalTransactions = data.length;
  const totalQuantity     = sumBy(data, 'Quantity_Sold');
  const avgDiscount       = data.length
    ? (sumBy(data, 'Discount') / data.length * 100).toFixed(1)
    : 0;

  document.getElementById('kpiRevenue').textContent      = formatCurrency(totalRevenue);
  document.getElementById('kpiTransactions').textContent = totalTransactions.toLocaleString();
  document.getElementById('kpiQuantity').textContent     = totalQuantity.toLocaleString();
  document.getElementById('kpiDiscount').textContent     = avgDiscount + '%';
  document.getElementById('recordCount').textContent     = totalTransactions + ' records';
}

// ─── CHART: SALES TREND (Line) ───────────────────────────────────────────────
// Monthly aggregated revenue
function renderSalesTrend(data) {
  destroyChart('salesTrend');

  // Map month numbers to abbreviations
  const MONTHS = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
  const monthly = {};
  MONTHS.forEach(m => monthly[m] = 0);

  data.forEach(row => {
    const month = new Date(row.Sale_Date).getMonth(); // 0-based
    monthly[MONTHS[month]] += row.Sales_Amount;
  });

  const ctx = document.getElementById('chartSalesTrend').getContext('2d');
  charts.salesTrend = new Chart(ctx, {
    type: 'line',
    data: {
      labels: MONTHS,
      datasets: [{
        label: 'Revenue',
        data: MONTHS.map(m => +monthly[m].toFixed(2)),
        borderColor: PALETTE[0],
        backgroundColor: PALETTE[0] + '20',
        borderWidth: 2.5,
        pointBackgroundColor: PALETTE[0],
        pointRadius: 4,
        pointHoverRadius: 6,
        fill: true,
        tension: 0.4
      }]
    },
    options: {
      ...baseOptions,
      scales: {
        y: {
          beginAtZero: true,
          ticks: {
            color: '#9ca3af',
            callback: v => '$' + (v / 1000).toFixed(0) + 'k'
          },
          grid: { color: '#f3f4f6' }
        },
        x: { ticks: { color: '#9ca3af' }, grid: { display: false } }
      },
      plugins: {
        ...baseOptions.plugins,
        legend: { display: false }
      }
    }
  });
}

// ─── CHART: REVENUE PER REGION (Bar) ─────────────────────────────────────────
function renderRevenueRegion(data) {
  destroyChart('region');

  const grouped = groupSum(data, 'Region', 'Sales_Amount');
  const labels  = Object.keys(grouped).sort();
  const values  = labels.map(l => +grouped[l].toFixed(2));

  const ctx = document.getElementById('chartRegion').getContext('2d');
  charts.region = new Chart(ctx, {
    type: 'bar',
    data: {
      labels,
      datasets: [{
        label: 'Revenue',
        data: values,
        backgroundColor: ALPHA_PALETTE.slice(0, labels.length),
        borderColor: PALETTE.slice(0, labels.length),
        borderWidth: 1.5,
        borderRadius: 6
      }]
    },
    options: {
      ...baseOptions,
      scales: {
        y: {
          beginAtZero: true,
          ticks: { color: '#9ca3af', callback: v => '$' + (v / 1000).toFixed(0) + 'k' },
          grid: { color: '#f3f4f6' }
        },
        x: { ticks: { color: '#9ca3af' }, grid: { display: false } }
      },
      plugins: { ...baseOptions.plugins, legend: { display: false } }
    }
  });
}

// ─── CHART: REVENUE PER PRODUCT CATEGORY (Horizontal Bar) ───────────────────
function renderRevenueCategory(data) {
  destroyChart('category');

  const grouped = groupSum(data, 'Product_Category', 'Sales_Amount');
  const labels  = Object.keys(grouped).sort();
  const values  = labels.map(l => +grouped[l].toFixed(2));

  const ctx = document.getElementById('chartCategory').getContext('2d');
  charts.category = new Chart(ctx, {
    type: 'bar',
    data: {
      labels,
      datasets: [{
        label: 'Revenue',
        data: values,
        backgroundColor: ALPHA_PALETTE.slice(0, labels.length),
        borderColor: PALETTE.slice(0, labels.length),
        borderWidth: 1.5,
        borderRadius: 6
      }]
    },
    options: {
      ...baseOptions,
      indexAxis: 'y',     // horizontal bars
      scales: {
        x: {
          beginAtZero: true,
          ticks: { color: '#9ca3af', callback: v => '$' + (v / 1000).toFixed(0) + 'k' },
          grid: { color: '#f3f4f6' }
        },
        y: { ticks: { color: '#6b7280' }, grid: { display: false } }
      },
      plugins: { ...baseOptions.plugins, legend: { display: false } }
    }
  });
}

// ─── CHART: SALES PER SALES REP (Bar) ────────────────────────────────────────
function renderSalesRep(data) {
  destroyChart('salesRep');

  const grouped = groupSum(data, 'Sales_Rep', 'Sales_Amount');
  // Sort by revenue descending
  const sorted  = Object.entries(grouped).sort((a, b) => b[1] - a[1]);
  const labels  = sorted.map(e => e[0]);
  const values  = sorted.map(e => +e[1].toFixed(2));

  const ctx = document.getElementById('chartSalesRep').getContext('2d');
  charts.salesRep = new Chart(ctx, {
    type: 'bar',
    data: {
      labels,
      datasets: [{
        label: 'Revenue',
        data: values,
        backgroundColor: ALPHA_PALETTE.slice(0, labels.length),
        borderColor: PALETTE.slice(0, labels.length),
        borderWidth: 1.5,
        borderRadius: 6
      }]
    },
    options: {
      ...baseOptions,
      scales: {
        y: {
          beginAtZero: true,
          ticks: { color: '#9ca3af', callback: v => '$' + (v / 1000).toFixed(0) + 'k' },
          grid: { color: '#f3f4f6' }
        },
        x: { ticks: { color: '#9ca3af' }, grid: { display: false } }
      },
      plugins: { ...baseOptions.plugins, legend: { display: false } }
    }
  });
}

// ─── CHART: CUSTOMER TYPE DISTRIBUTION (Doughnut) ────────────────────────────
function renderCustomerType(data) {
  destroyChart('customerType');

  const counted = countBy(data, 'Customer_Type');
  const labels  = Object.keys(counted);
  const values  = labels.map(l => counted[l]);

  const ctx = document.getElementById('chartCustomerType').getContext('2d');
  charts.customerType = new Chart(ctx, {
    type: 'doughnut',
    data: {
      labels,
      datasets: [{
        data: values,
        backgroundColor: [PALETTE[0] + 'cc', PALETTE[3] + 'cc'],
        borderColor: ['#fff', '#fff'],
        borderWidth: 3,
        hoverOffset: 6
      }]
    },
    options: {
      ...baseOptions,
      cutout: '65%',
      plugins: {
        ...baseOptions.plugins,
        legend: { position: 'bottom', labels: { ...baseOptions.plugins.legend.labels, padding: 16 } }
      }
    }
  });
}

// ─── CHART: PAYMENT METHOD USAGE (Pie) ───────────────────────────────────────
function renderPaymentMethod(data) {
  destroyChart('payment');

  const counted = countBy(data, 'Payment_Method');
  const labels  = Object.keys(counted);
  const values  = labels.map(l => counted[l]);

  const ctx = document.getElementById('chartPayment').getContext('2d');
  charts.payment = new Chart(ctx, {
    type: 'pie',
    data: {
      labels,
      datasets: [{
        data: values,
        backgroundColor: ALPHA_PALETTE.slice(0, labels.length),
        borderColor: '#fff',
        borderWidth: 2,
        hoverOffset: 6
      }]
    },
    options: {
      ...baseOptions,
      plugins: {
        ...baseOptions.plugins,
        legend: { position: 'bottom', labels: { ...baseOptions.plugins.legend.labels, padding: 12 } }
      }
    }
  });
}

// ─── CHART: SALES CHANNEL DISTRIBUTION (Doughnut) ───────────────────────────
function renderSalesChannel(data) {
  destroyChart('channel');

  const counted = countBy(data, 'Sales_Channel');
  const labels  = Object.keys(counted);
  const values  = labels.map(l => counted[l]);

  const ctx = document.getElementById('chartChannel').getContext('2d');
  charts.channel = new Chart(ctx, {
    type: 'doughnut',
    data: {
      labels,
      datasets: [{
        data: values,
        backgroundColor: [PALETTE[1] + 'cc', PALETTE[4] + 'cc'],
        borderColor: ['#fff', '#fff'],
        borderWidth: 3,
        hoverOffset: 6
      }]
    },
    options: {
      ...baseOptions,
      cutout: '65%',
      plugins: {
        ...baseOptions.plugins,
        legend: { position: 'bottom', labels: { ...baseOptions.plugins.legend.labels, padding: 16 } }
      }
    }
  });
}

// ─── CHART: DISCOUNT VS SALES AMOUNT (Scatter) ───────────────────────────────
function renderScatter(data) {
  destroyChart('scatter');

  // Map each record to {x: discount%, y: salesAmount}
  const points = data.map(row => ({
    x: +(row.Discount * 100).toFixed(1),
    y: +row.Sales_Amount.toFixed(2)
  }));

  const ctx = document.getElementById('chartScatter').getContext('2d');
  charts.scatter = new Chart(ctx, {
    type: 'scatter',
    data: {
      datasets: [{
        label: 'Transaction',
        data: points,
        backgroundColor: PALETTE[0] + '66',
        borderColor: PALETTE[0],
        borderWidth: 1,
        pointRadius: 5,
        pointHoverRadius: 7
      }]
    },
    options: {
      ...baseOptions,
      scales: {
        x: {
          title: { display: true, text: 'Discount (%)', color: '#6b7280', font: { size: 11 } },
          ticks: { color: '#9ca3af', callback: v => v + '%' },
          grid: { color: '#f3f4f6' }
        },
        y: {
          title: { display: true, text: 'Sales Amount ($)', color: '#6b7280', font: { size: 11 } },
          ticks: { color: '#9ca3af', callback: v => '$' + (v / 1000).toFixed(0) + 'k' },
          grid: { color: '#f3f4f6' }
        }
      },
      plugins: { ...baseOptions.plugins, legend: { display: false } }
    }
  });
}

// ─── CHART: TOP 10 HIGHEST SALES (Horizontal Bar) ────────────────────────────
function renderTop10(data) {
  destroyChart('top10');

  // Sort descending, take first 10
  const top10 = [...data]
    .sort((a, b) => b.Sales_Amount - a.Sales_Amount)
    .slice(0, 10);

  // Label: "Rep · Category · Date"
  const labels = top10.map(r =>
    `${r.Sales_Rep} · ${r.Product_Category} · ${r.Sale_Date}`
  );
  const values = top10.map(r => +r.Sales_Amount.toFixed(2));

  const ctx = document.getElementById('chartTop10').getContext('2d');
  charts.top10 = new Chart(ctx, {
    type: 'bar',
    data: {
      labels,
      datasets: [{
        label: 'Sales Amount',
        data: values,
        backgroundColor: PALETTE.map(c => c + 'bb'),
        borderColor: PALETTE,
        borderWidth: 1.5,
        borderRadius: 4
      }]
    },
    options: {
      ...baseOptions,
      indexAxis: 'y',
      scales: {
        x: {
          beginAtZero: true,
          ticks: { color: '#9ca3af', callback: v => '$' + (v / 1000).toFixed(0) + 'k' },
          grid: { color: '#f3f4f6' }
        },
        y: { ticks: { color: '#6b7280', font: { size: 10 } }, grid: { display: false } }
      },
      plugins: { ...baseOptions.plugins, legend: { display: false } }
    }
  });
}

// ─── CHART: QUANTITY SOLD PER CATEGORY (Bar) ─────────────────────────────────
function renderQtyCategory(data) {
  destroyChart('qtyCategory');

  const grouped = groupSum(data, 'Product_Category', 'Quantity_Sold');
  const sorted  = Object.entries(grouped).sort((a, b) => b[1] - a[1]);
  const labels  = sorted.map(e => e[0]);
  const values  = sorted.map(e => e[1]);

  const ctx = document.getElementById('chartQtyCategory').getContext('2d');
  charts.qtyCategory = new Chart(ctx, {
    type: 'bar',
    data: {
      labels,
      datasets: [{
        label: 'Units Sold',
        data: values,
        backgroundColor: ALPHA_PALETTE.slice(0, labels.length),
        borderColor: PALETTE.slice(0, labels.length),
        borderWidth: 1.5,
        borderRadius: 6
      }]
    },
    options: {
      ...baseOptions,
      scales: {
        y: {
          beginAtZero: true,
          ticks: { color: '#9ca3af' },
          grid: { color: '#f3f4f6' }
        },
        x: { ticks: { color: '#9ca3af' }, grid: { display: false } }
      },
      plugins: { ...baseOptions.plugins, legend: { display: false } }
    }
  });
}

// ─── MAIN UPDATE FUNCTION ─────────────────────────────────────────────────────
// Called once on load, and again on every filter change
function updateDashboard(fullData) {
  const filtered = applyFilters(fullData);

  updateKPIs(filtered);
  renderSalesTrend(filtered);
  renderRevenueRegion(filtered);
  renderRevenueCategory(filtered);
  renderSalesRep(filtered);
  renderCustomerType(filtered);
  renderPaymentMethod(filtered);
  renderSalesChannel(filtered);
  renderScatter(filtered);
  renderTop10(filtered);
  renderQtyCategory(filtered);
}

// ─── ENTRY POINT ─────────────────────────────────────────────────────────────
// Fetch JSON, wire up filters, do the first render
fetch('sales_data.json')
  .then(res => {
    if (!res.ok) throw new Error('Failed to load sales_data.json');
    return res.json();
  })
  .then(data => {
    // Populate filter dropdowns with unique values from the dataset
    populateFilters(data);

    // Attach change listeners to all filter selects
    ['filterRegion', 'filterCategory', 'filterCustomer', 'filterChannel'].forEach(id => {
      document.getElementById(id).addEventListener('change', () => updateDashboard(data));
    });

    // Reset button restores all selects to "all" and re-renders
    document.getElementById('resetFilters').addEventListener('click', () => {
      ['filterRegion', 'filterCategory', 'filterCustomer', 'filterChannel'].forEach(id => {
        document.getElementById(id).value = 'all';
      });
      updateDashboard(data);
    });

    // Initial render with full dataset
    updateDashboard(data);
  })
  .catch(err => {
    console.error('Dashboard error:', err);
    document.body.insertAdjacentHTML('afterbegin',
      `<div class="bg-red-50 text-red-700 text-sm p-4 border-b border-red-200">
        ⚠️ Could not load <code>sales_data.json</code>. Make sure you are serving this page via a local server (e.g. <code>npx serve .</code> or VS Code Live Server).
      </div>`
    );
  });
