/**
 * Main Dashboard JavaScript
 * Common utilities and functions for all pages
 */

/**
 * Show an alert message
 * @param {string} type - Alert type (success, danger, warning, info)
 * @param {string} message - Alert message
 * @param {number} duration - Auto-dismiss duration in ms (0 = no auto-dismiss)
 */
function showAlert(type, message, duration = 5000) {
    const alertsContainer = document.getElementById('alerts-container') || createAlertsContainer();

    const alertId = 'alert-' + Date.now();
    const alertHTML = `
        <div id="${alertId}" class="alert alert-${type} alert-dismissible fade show" role="alert">
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        </div>
    `;

    alertsContainer.insertAdjacentHTML('beforeend', alertHTML);

    if (duration > 0) {
        setTimeout(() => {
            const alert = document.getElementById(alertId);
            if (alert) {
                const bsAlert = new bootstrap.Alert(alert);
                bsAlert.close();
            }
        }, duration);
    }
}

/**
 * Create alerts container if it doesn't exist
 */
function createAlertsContainer() {
    const main = document.querySelector('main');
    const container = document.createElement('div');
    container.id = 'alerts-container';
    container.className = 'alerts-container';
    main.insertBefore(container, main.firstChild);
    return container;
}

/**
 * Format timestamp to localized date/time
 * @param {number} timestamp - Unix timestamp
 * @returns {string} Formatted date/time
 */
function formatTimestamp(timestamp) {
    if (!timestamp || timestamp === 0) return '-';
    const date = new Date(timestamp * 1000);
    return date.toLocaleString();
}

/**
 * Format GM level to human-readable name
 * @param {number} level - GM level number
 * @returns {string} GM level name
 */
function formatGMLevel(level) {
    const levels = {
        0: 'Civilian',
        1: 'Forum Moderator',
        2: 'Junior Moderator',
        3: 'Moderator',
        4: 'Senior Moderator',
        5: 'Lead Moderator',
        6: 'Junior Developer',
        7: 'Inactive Developer',
        8: 'Developer',
        9: 'Operator'
    };
    return levels[level] || 'Unknown';
}

/**
 * Confirm action with modal
 * @param {string} title - Modal title
 * @param {string} message - Modal message
 * @param {function} callback - Callback function if confirmed
 */
function confirmAction(title, message, callback) {
    if (confirm(message)) {
        callback();
    }
}

/**
 * Copy text to clipboard
 * @param {string} text - Text to copy
 */
async function copyToClipboard(text) {
    try {
        await navigator.clipboard.writeText(text);
        showAlert('success', 'Copied to clipboard!', 2000);
    } catch (err) {
        showAlert('danger', 'Failed to copy to clipboard');
    }
}

/**
 * Debounce function calls
 * @param {function} func - Function to debounce
 * @param {number} wait - Wait time in ms
 * @returns {function} Debounced function
 */
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

/**
 * Initialize DataTables default settings
 */
$.extend(true, $.fn.dataTable.defaults, {
    responsive: true,
    lengthMenu: [[10, 25, 50, 100, -1], [10, 25, 50, 100, "All"]],
    pageLength: 25,
    language: {
        search: "_INPUT_",
        searchPlaceholder: "Search...",
        lengthMenu: "Show _MENU_ entries",
        info: "Showing _START_ to _END_ of _TOTAL_ entries",
        infoEmpty: "No entries found",
        infoFiltered: "(filtered from _MAX_ total entries)",
        zeroRecords: "No matching records found",
        emptyTable: "No data available in table"
    }
});

/**
 * Handle form submission with API
 * @param {string} formId - Form element ID
 * @param {string} endpoint - API endpoint
 * @param {function} onSuccess - Success callback
 */
function handleFormSubmit(formId, endpoint, onSuccess) {
    const form = document.getElementById(formId);
    if (!form) return;

    form.addEventListener('submit', async (e) => {
        e.preventDefault();

        const formData = new FormData(form);
        const data = Object.fromEntries(formData);

        try {
            const result = await API.post(endpoint, data);

            if (result.success) {
                showAlert('success', result.message || 'Operation successful');
                if (onSuccess) onSuccess(result);
            } else {
                showAlert('danger', result.error || 'Operation failed');
            }
        } catch (error) {
            showAlert('danger', error.message);
        }
    });
}

/**
 * Initialize tooltips
 */
document.addEventListener('DOMContentLoaded', function() {
    // Initialize Bootstrap tooltips
    const tooltipTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="tooltip"]'));
    tooltipTriggerList.map(function(tooltipTriggerEl) {
        return new bootstrap.Tooltip(tooltipTriggerEl);
    });

    // Initialize Bootstrap popovers
    const popoverTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="popover"]'));
    popoverTriggerList.map(function(popoverTriggerEl) {
        return new bootstrap.Popover(popoverTriggerEl);
    });
});
