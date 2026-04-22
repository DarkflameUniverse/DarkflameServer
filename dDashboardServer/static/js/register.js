document.addEventListener('DOMContentLoaded', () => {
    const form = document.getElementById('register-form');
    const alertBox = document.getElementById('register-alert');

    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        alertBox.style.display = 'none';
        const username = document.getElementById('username').value.trim();
        const password = document.getElementById('password').value;
        const play_key = document.getElementById('play_key').value.trim();

        try {
            const res = await fetch('/api/register', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password, play_key })
            });

            const data = await res.json();
            if (!res.ok) {
                alertBox.className = 'alert alert-danger';
                alertBox.textContent = data.error || 'Registration failed';
                alertBox.style.display = 'block';
                return;
            }

            if (data.success) {
                alertBox.className = 'alert alert-success';
                alertBox.textContent = 'Account created successfully. You can now log in.';
                alertBox.style.display = 'block';
                form.reset();
            } else {
                alertBox.className = 'alert alert-danger';
                alertBox.textContent = data.error || 'Registration failed';
                alertBox.style.display = 'block';
            }
        } catch (err) {
            alertBox.className = 'alert alert-danger';
            alertBox.textContent = err.message || 'Registration failed';
            alertBox.style.display = 'block';
        }
    });
});
