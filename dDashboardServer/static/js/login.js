(function () {
	'use strict';

	function showAlert(message, type) {
		var el = document.getElementById('alert');
		el.textContent = message;
		el.className = 'alert alert-' + (type === 'error' ? 'danger' : 'success');
	}

	function setLoading(loading) {
		document.getElementById('loading').classList.toggle('d-none', !loading);
		document.getElementById('loginBtn').disabled = loading;
	}

	function post(url, body) {
		return fetch(url, {
			method: 'POST',
			credentials: 'same-origin',
			headers: { 'Content-Type': 'application/json', 'X-Requested-With': 'dashboard' },
			body: JSON.stringify(body)
		}).then(function (r) { return r.json(); });
	}

	document.addEventListener('DOMContentLoaded', function () {
		var form = document.getElementById('loginForm');
		if (!form) return;
		var twoFactorForm = document.getElementById('twoFactorForm');
		var challenge = null;

		function showTwoFactor(show) {
			form.classList.toggle('d-none', show);
			twoFactorForm.classList.toggle('d-none', !show);
			document.getElementById('alert').className = 'alert d-none';
			if (show) document.getElementById('twoFactorCode').focus();
		}

		twoFactorForm.addEventListener('submit', function (e) {
			e.preventDefault();
			var button = document.getElementById('twoFactorBtn');
			button.disabled = true;
			post('/api/auth/login/2fa', { challenge: challenge, code: document.getElementById('twoFactorCode').value.trim() })
				.then(function (data) {
					if (data.success) { window.location.href = '/'; return; }
					showAlert(data.message || 'That code is not right', 'error');
					document.getElementById('twoFactorCode').select();
					// An expired or used-up challenge means starting over
					if (/expired|sign in again|locked/i.test(data.message || '')) {
						showTwoFactor(false);
						showAlert(data.message, 'error');
					}
				})
				.catch(function (err) { showAlert('Network error: ' + err.message, 'error'); })
				.finally(function () { button.disabled = false; });
		});
		document.getElementById('twoFactorBack').addEventListener('click', function () { challenge = null; showTwoFactor(false); });

		fetch('/api/auth/config').then(function (r) { return r.json(); }).then(function (config) {
			if (config.emailEnabled || config.recoveryReset) document.getElementById('forgotLink').classList.remove('d-none');
			// Pages anyone may open, when the server has them on
			if (config.publicStatus || config.publicShowcase) document.getElementById('publicLinks').classList.remove('d-none');
			if (config.publicStatus) document.getElementById('publicStatusLink').classList.remove('d-none');
			if (config.publicShowcase) document.getElementById('publicShowcaseLink').classList.remove('d-none');
			if (config.publicStatus && config.publicShowcase) document.getElementById('publicLinksDot').classList.remove('d-none');
			if (!config.registration) return;
			document.getElementById('registerLink').classList.remove('d-none');
			document.getElementById('noAccessText').classList.add('d-none');
		}).catch(function () {});

		form.addEventListener('submit', function (e) {
			e.preventDefault();
			var username = document.getElementById('username').value.trim();
			var password = document.getElementById('password').value;
			var rememberMe = document.getElementById('rememberMe').checked;

			if (!username || !password) { showAlert('Username and password are required', 'error'); return; }
			if (password.length > 40) { showAlert('Password exceeds maximum length', 'error'); return; }

			setLoading(true);
			// The server sets an HttpOnly session cookie; the token in the response is for API clients only
			post('/api/auth/login', { username: username, password: password, rememberMe: rememberMe })
				.then(function (data) {
					if (data.success) {
						window.location.href = '/';
					} else if (data.twoFactorRequired) {
						challenge = data.challenge;
						setLoading(false);
						showTwoFactor(true);
					} else {
						showAlert(data.message || 'Login failed', 'error');
						setLoading(false);
					}
				})
				.catch(function (err) {
					showAlert('Network error: ' + err.message, 'error');
					setLoading(false);
				});
		});
	});
})();
