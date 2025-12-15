// Dashboard JavaScript - Real-time WebSocket Updates

class TradingDashboard {
    constructor() {
        this.ws = null;
        this.reconnectInterval = 5000;
        this.charts = {};
        this.dataBuffers = {
            pnl: [],
            price: [],
            spread: [],
            buyIntensity: [],
            sellIntensity: [],
            latency: [],
            position: [],
            orders: []
        };
        this.maxDataPoints = 100;
        
        this.initializeCharts();
        this.connect();
    }

    connect() {
        try {
            this.ws = new WebSocket('ws://localhost:8080');
            
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.updateConnectionStatus(true);
                
                // Request historical data
                this.ws.send(JSON.stringify({command: 'get_history'}));
            };
            
            this.ws.onmessage = (event) => {
                const data = JSON.parse(event.data);
                this.handleMessage(data);
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.updateConnectionStatus(false);
            };
            
            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.updateConnectionStatus(false);
                
                // Attempt reconnection
                setTimeout(() => this.connect(), this.reconnectInterval);
            };
        } catch (error) {
            console.error('Connection error:', error);
            setTimeout(() => this.connect(), this.reconnectInterval);
        }
    }

    handleMessage(data) {
        if (Array.isArray(data)) {
            // Historical data
            this.loadHistoricalData(data);
        } else if (data.type === 'update') {
            // Real-time update
            this.updateMetrics(data);
            this.updateCharts(data);
        } else if (data.type === 'summary') {
            this.updateSummary(data);
        }
    }

    loadHistoricalData(dataArray) {
        // Clear existing buffers
        Object.keys(this.dataBuffers).forEach(key => {
            this.dataBuffers[key] = [];
        });

        // Load historical data
        dataArray.forEach(point => {
            const timestamp = new Date(point.timestamp / 1000000);  // ns to ms
            
            this.dataBuffers.pnl.push({x: timestamp, y: point.pnl});
            this.dataBuffers.price.push({x: timestamp, y: point.mid_price});
            this.dataBuffers.spread.push({x: timestamp, y: point.spread});
            this.dataBuffers.buyIntensity.push({x: timestamp, y: point.buy_intensity});
            this.dataBuffers.sellIntensity.push({x: timestamp, y: point.sell_intensity});
            this.dataBuffers.latency.push({x: timestamp, y: point.latency});
            this.dataBuffers.position.push({x: timestamp, y: point.position});
        });

        // Update all charts with historical data
        this.refreshAllCharts();
    }

    updateMetrics(data) {
        // Update metric cards
        document.getElementById('pnlValue').textContent = `$${data.pnl.toFixed(2)}`;
        document.getElementById('positionValue').textContent = data.position;
        document.getElementById('midPriceValue').textContent = `$${data.mid_price.toFixed(2)}`;
        document.getElementById('spreadValue').textContent = `Spread: ${data.spread.toFixed(2)} bps`;
        document.getElementById('ordersFilledValue').textContent = data.orders_filled;
        document.getElementById('avgLatencyValue').textContent = `${data.latency.toFixed(1)} μs`;
        document.getElementById('latencyDisplay').textContent = `${data.latency.toFixed(0)} μs`;
        
        // Fill rate
        const fillRate = data.orders_sent > 0 ? 
            ((data.orders_filled / data.orders_sent) * 100).toFixed(1) : 0;
        document.getElementById('fillRateValue').textContent = `Fill Rate: ${fillRate}%`;
        
        // Position usage
        document.getElementById('positionUsage').textContent = 
            `${data.position_usage.toFixed(1)}% of limit`;
        
        // Hawkes intensity
        const totalIntensity = data.buy_intensity + data.sell_intensity;
        document.getElementById('intensityValue').textContent = totalIntensity.toFixed(2);
        
        const imbalance = totalIntensity > 0 ? 
            ((data.buy_intensity - data.sell_intensity) / totalIntensity * 100) : 0;
        document.getElementById('intensityImbalance').textContent = 
            `Imbalance: ${imbalance > 0 ? '+' : ''}${imbalance.toFixed(1)}%`;
        
        // Update regime
        this.updateRegime(data.regime);
        
        // Add to log
        this.addLogEntry(data);
    }

    updateCharts(data) {
        const timestamp = new Date();
        
        // Add new data points
        this.dataBuffers.pnl.push({x: timestamp, y: data.pnl});
        this.dataBuffers.price.push({x: timestamp, y: data.mid_price});
        this.dataBuffers.spread.push({x: timestamp, y: data.spread});
        this.dataBuffers.buyIntensity.push({x: timestamp, y: data.buy_intensity});
        this.dataBuffers.sellIntensity.push({x: timestamp, y: data.sell_intensity});
        this.dataBuffers.latency.push({x: timestamp, y: data.latency});
        this.dataBuffers.position.push({x: timestamp, y: data.position});
        
        // Trim old data
        Object.keys(this.dataBuffers).forEach(key => {
            if (this.dataBuffers[key].length > this.maxDataPoints) {
                this.dataBuffers[key].shift();
            }
        });
        
        // Update charts
        this.refreshAllCharts();
    }

    refreshAllCharts() {
        // P&L Chart
        this.charts.pnl.data.datasets[0].data = this.dataBuffers.pnl;
        this.charts.pnl.update('none');
        
        // Price Chart
        this.charts.price.data.datasets[0].data = this.dataBuffers.price;
        this.charts.price.data.datasets[1].data = this.dataBuffers.spread;
        this.charts.price.update('none');
        
        // Hawkes Chart
        this.charts.hawkes.data.datasets[0].data = this.dataBuffers.buyIntensity;
        this.charts.hawkes.data.datasets[1].data = this.dataBuffers.sellIntensity;
        this.charts.hawkes.update('none');
        
        // Latency Chart
        this.charts.latency.data.datasets[0].data = this.dataBuffers.latency;
        this.charts.latency.update('none');
        
        // Position Chart
        this.charts.position.data.datasets[0].data = this.dataBuffers.position;
        this.charts.position.update('none');
    }

    updateRegime(regime) {
        const regimeNames = ['NORMAL', 'ELEVATED', 'STRESS', 'HALTED'];
        const regimeClasses = ['regime-normal', 'regime-elevated', 'regime-stress', 'regime-halted'];
        
        const regimeDisplay = document.getElementById('regimeDisplay');
        regimeDisplay.textContent = regimeNames[regime];
        regimeDisplay.className = 'regime-badge ' + regimeClasses[regime];
    }

    updateConnectionStatus(connected) {
        const statusIndicator = document.getElementById('wsStatus');
        const connectionText = document.getElementById('connectionText');
        
        if (connected) {
            statusIndicator.className = 'status-indicator connected';
            connectionText.textContent = 'Connected';
        } else {
            statusIndicator.className = 'status-indicator disconnected';
            connectionText.textContent = 'Disconnected';
        }
    }

    addLogEntry(data) {
        const container = document.getElementById('orderLogContainer');
        const entry = document.createElement('div');
        entry.className = 'order-entry';
        
        const time = new Date().toLocaleTimeString();
        const pnlColor = data.pnl >= 0 ? '#00ff88' : '#ff4444';
        
        entry.innerHTML = `
            <div class="order-time">${time}</div>
            <div>Position: ${data.position} | P&L: <span style="color: ${pnlColor}">$${data.pnl.toFixed(2)}</span></div>
            <div>Mid: $${data.mid_price.toFixed(2)} | Latency: ${data.latency.toFixed(1)}μs</div>
        `;
        
        container.insertBefore(entry, container.firstChild);
        
        // Keep only last 20 entries
        while (container.children.length > 20) {
            container.removeChild(container.lastChild);
        }
    }

    initializeCharts() {
        const chartColors = {
            primary: 'rgba(102, 126, 234, 1)',
            secondary: 'rgba(118, 75, 162, 1)',
            success: 'rgba(0, 255, 136, 1)',
            danger: 'rgba(255, 68, 68, 1)',
            warning: 'rgba(255, 165, 0, 1)',
        };

        const commonOptions = {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
            plugins: {
                legend: {
                    labels: {
                        color: '#fff',
                        font: {size: 12}
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: '#fff',
                    bodyColor: '#fff'
                }
            },
            scales: {
                x: {
                    type: 'time',
                    time: {
                        unit: 'second',
                        displayFormats: {
                            second: 'HH:mm:ss'
                        }
                    },
                    grid: {
                        color: 'rgba(255, 255, 255, 0.1)'
                    },
                    ticks: {
                        color: '#fff'
                    }
                },
                y: {
                    grid: {
                        color: 'rgba(255, 255, 255, 0.1)'
                    },
                    ticks: {
                        color: '#fff'
                    }
                }
            }
        };

        // P&L Chart
        this.charts.pnl = new Chart(document.getElementById('pnlChart'), {
            type: 'line',
            data: {
                datasets: [{
                    label: 'P&L ($)',
                    data: [],
                    borderColor: chartColors.success,
                    backgroundColor: 'rgba(0, 255, 136, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });

        // Price & Spread Chart
        this.charts.price = new Chart(document.getElementById('priceChart'), {
            type: 'line',
            data: {
                datasets: [
                    {
                        label: 'Mid Price ($)',
                        data: [],
                        borderColor: chartColors.primary,
                        yAxisID: 'y',
                        tension: 0.4
                    },
                    {
                        label: 'Spread (bps)',
                        data: [],
                        borderColor: chartColors.warning,
                        yAxisID: 'y1',
                        tension: 0.4
                    }
                ]
            },
            options: {
                ...commonOptions,
                scales: {
                    ...commonOptions.scales,
                    y1: {
                        type: 'linear',
                        position: 'right',
                        grid: {
                            drawOnChartArea: false
                        },
                        ticks: {
                            color: '#fff'
                        }
                    }
                }
            }
        });

        // Hawkes Process Chart
        this.charts.hawkes = new Chart(document.getElementById('hawkesChart'), {
            type: 'line',
            data: {
                datasets: [
                    {
                        label: 'Buy Intensity',
                        data: [],
                        borderColor: chartColors.success,
                        backgroundColor: 'rgba(0, 255, 136, 0.1)',
                        fill: true,
                        tension: 0.4
                    },
                    {
                        label: 'Sell Intensity',
                        data: [],
                        borderColor: chartColors.danger,
                        backgroundColor: 'rgba(255, 68, 68, 0.1)',
                        fill: true,
                        tension: 0.4
                    }
                ]
            },
            options: commonOptions
        });

        // Latency Chart
        this.charts.latency = new Chart(document.getElementById('latencyChart'), {
            type: 'line',
            data: {
                datasets: [{
                    label: 'Cycle Latency (μs)',
                    data: [],
                    borderColor: chartColors.secondary,
                    backgroundColor: 'rgba(118, 75, 162, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });

        // Position Chart
        this.charts.position = new Chart(document.getElementById('positionChart'), {
            type: 'line',
            data: {
                datasets: [{
                    label: 'Position',
                    data: [],
                    borderColor: chartColors.primary,
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: commonOptions
        });

        // Order Flow Chart (placeholder - will show orders sent/filled over time)
        this.charts.orderFlow = new Chart(document.getElementById('orderFlowChart'), {
            type: 'bar',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Orders Sent',
                        data: [],
                        backgroundColor: 'rgba(102, 126, 234, 0.6)'
                    },
                    {
                        label: 'Orders Filled',
                        data: [],
                        backgroundColor: 'rgba(0, 255, 136, 0.6)'
                    }
                ]
            },
            options: {
                ...commonOptions,
                scales: {
                    x: {
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: '#fff'
                        }
                    },
                    y: {
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: '#fff'
                        }
                    }
                }
            }
        });
    }
}

// Initialize dashboard when page loads
window.addEventListener('DOMContentLoaded', () => {
    const dashboard = new TradingDashboard();
    
    // Log initialization
    console.log('HFT Trading Dashboard initialized');
    console.log('Connecting to WebSocket server...');
});
