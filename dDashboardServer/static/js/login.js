/**
 * Login page functionality
 */

// Function to initialize login form
function initLoginForm() {
    const form = document.getElementById('login-form');
    if (!form) return; // Not on login page

    form.addEventListener('submit', async (e) => {
        e.preventDefault();

        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        const messageDiv = document.getElementById('login-message');

        try {
            const response = await API.post('/api/login', { username, password });

            if (response && response.success) {
                messageDiv.className = 'alert alert-success';
                messageDiv.textContent = 'Login successful! Redirecting...';
                messageDiv.style.display = 'block';

                setTimeout(() => {
                    window.location.href = '/';
                }, 1000);
            } else {
                messageDiv.className = 'alert alert-danger';
                messageDiv.textContent = response.error || 'Login failed';
                messageDiv.style.display = 'block';
            }
        } catch (error) {
            messageDiv.className = 'alert alert-danger';
            messageDiv.textContent = error.message || 'An error occurred during login';
            messageDiv.style.display = 'block';
        }
    });
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initLoginForm);
} else {
    initLoginForm();
}
