// Helper to wait for jQuery and DataTables (and optionally API) to be available
// Usage:
//   safeInit(callback, { timeout: 5000, interval: 100, requireApi: false })
// The callback receives `window.jQuery` as its first argument.
(function(window) {
    'use strict';

    function waitFor(conditionFn, timeoutMs, intervalMs) {
        return new Promise((resolve, reject) => {
            const start = Date.now();
            const iv = setInterval(() => {
                try {
                    if (conditionFn()) {
                        clearInterval(iv);
                        resolve();
                        return;
                    }
                } catch (e) {
                    // ignore
                }
                if (Date.now() - start > timeoutMs) {
                    clearInterval(iv);
                    reject(new Error('waitFor: timed out'));
                }
            }, intervalMs);
        });
    }

    async function safeInit(cb, opts) {
        opts = opts || {};
        const timeout = typeof opts.timeout === 'number' ? opts.timeout : 5000;
        const interval = typeof opts.interval === 'number' ? opts.interval : 100;
        const requireApi = !!opts.requireApi;

        // Wait for DOM ready first so scripts included at end of body have run
        if (document.readyState === 'loading') {
            await new Promise(r => document.addEventListener('DOMContentLoaded', r, { once: true }));
        }

        try {
            await waitFor(() => window.jQuery && window.jQuery.fn && window.jQuery.fn.DataTable, timeout, interval);
            if (requireApi) {
                await waitFor(() => window.API, timeout, interval);
            }
            // call callback with jQuery
            try { cb(window.jQuery); } catch (e) { console.error('safeInit callback error', e); }
        } catch (err) {
            console.error('safeInit: required libraries failed to load', err);
            // If callback provided an onError handler, call it
            if (opts.onError && typeof opts.onError === 'function') {
                try { opts.onError(err); } catch (e) { console.error(e); }
            } else {
                // default fallback: show a banner if possible
                const tableEls = document.querySelectorAll('table');
                if (tableEls && tableEls.length) {
                    tableEls.forEach(el => {
                        const wrapper = document.createElement('div');
                        wrapper.className = 'alert alert-danger';
                        wrapper.textContent = 'Required JavaScript libraries failed to load (jQuery/DataTables). Please check your network or CDN allowlist.';
                        el.replaceWith(wrapper);
                    });
                } else {
                    console.warn('safeInit: libraries missing');
                }
            }
        }
    }

    // Expose globally
    window.safeInit = safeInit;
    window.waitForLibraries = function(timeoutMs, intervalMs) {
        return waitFor(() => window.jQuery && window.jQuery.fn && window.jQuery.fn.DataTable, timeoutMs || 5000, intervalMs || 100);
    };

})(window);
