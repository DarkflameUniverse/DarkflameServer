// Check if user is already logged in
function checkExistingToken() {
	const token = localStorage.getItem('dashboardToken');
	if (token) {
		verifyTokenAndRedirect(token);
	}
}

function verifyTokenAndRedirect(token) {
	fetch('/api/auth/verify', {
		method: 'POST',
		headers: { 'Content-Type': 'application/json' },
		body: JSON.stringify({ token: token })
	})
	.then(res => res.json())
	.then(data => {
		if (data.valid) {
			window.location.href = '/';
		}
	})
	.catch(err => console.error('Token verification failed:', err));
}

function showAlert(message, type) {
	const alert = document.getElementById('alert');
	alert.textContent = message;
	alert.className = 'alert';
	if (type === 'error') {
		alert.classList.add('alert-danger');
	} else if (type === 'success') {
		alert.classList.add('alert-success');
	}
	alert.style.display = 'block';
}

// Wait for DOM to be ready
document.addEventListener('DOMContentLoaded', () => {
	const loginForm = document.getElementById('loginForm');
	if (!loginForm) {
		console.error('Login form not found');
		return;
	}

	loginForm.addEventListener('submit', async (e) => {
		e.preventDefault();

		const username = document.getElementById('username').value;
		const password = document.getElementById('password').value;
		const rememberMe = document.getElementById('rememberMe').checked;

		// Validate input
		if (!username || !password) {
			showAlert('Username and password are required', 'error');
			return;
		}

		if (password.length > 40) {
			showAlert('Password exceeds maximum length (40 characters)', 'error');
			return;
		}

		// Show loading state
		document.getElementById('loading').style.display = 'inline-block';
		document.getElementById('loginBtn').disabled = true;

		try {
			const response = await fetch('/api/auth/login', {
				method: 'POST',
				headers: { 'Content-Type': 'application/json' },
				body: JSON.stringify({ username, password, rememberMe })
			});

			const data = await response.json();

			if (data.success) {
				// Store token in localStorage (also set as cookie for API calls)
				localStorage.setItem('dashboardToken', data.token);
				document.cookie = `dashboardToken=${data.token}; path=/; SameSite=Strict`;
				showAlert('Login successful! Redirecting...', 'success');
				
				// Redirect after a short delay (no token in URL)
				setTimeout(() => {
					window.location.href = '/';
				}, 1000);
			} else {
				showAlert(data.message || 'Login failed', 'error');
				document.getElementById('loading').style.display = 'none';
				document.getElementById('loginBtn').disabled = false;
			}
		} catch (error) {
			showAlert('Network error: ' + error.message, 'error');
			document.getElementById('loading').style.display = 'none';
			document.getElementById('loginBtn').disabled = false;
		}
	});

	// Check existing token on page load
	checkExistingToken();
});
