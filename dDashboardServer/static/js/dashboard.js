let ws = null;
let reconnectAttempts = 0;
const maxReconnectAttempts = 5;
const reconnectDelay = 3000;

// Helper function to get cookie value
function getCookie(name) {
	const nameEQ = name + '=';
	const cookies = document.cookie.split(';');
	for (let cookie of cookies) {
		cookie = cookie.trim();
		if (cookie.indexOf(nameEQ) === 0) {
			return decodeURIComponent(cookie.substring(nameEQ.length));
		}
	}
	return null;
}

// Helper function to delete cookie
function deleteCookie(name) {
	document.cookie = `${name}=; expires=Thu, 01 Jan 1970 00:00:00 UTC; path=/; SameSite=Strict`;
}

// Check authentication on page load
function checkAuthentication() {
	// Check localStorage first (most secure)
	let token = localStorage.getItem('dashboardToken');
	
	// Fallback to cookie if localStorage empty
	if (!token) {
		token = getCookie('dashboardToken');
	}
	
	if (!token) {
		// Redirect to login if no token
		window.location.href = '/login';
		return false;
	}
	
	// Verify token is valid (asynchronous)
	fetch('/api/auth/verify', {
		method: 'POST',
		headers: { 'Content-Type': 'application/json' },
		body: JSON.stringify({ token: token })
	})
	.then(res => {
		if (!res.ok) {
			console.error('Verify endpoint returned:', res.status);
			throw new Error(`HTTP ${res.status}`);
		}
		return res.json();
	})
	.then(data => {
		console.log('Token verification response:', data);
		if (!data.valid) {
			// Token is invalid/expired, delete cookies and redirect to login
			console.log('Token verification failed, redirecting to login');
			deleteCookie('dashboardToken');
			deleteCookie('gmLevel');
			localStorage.removeItem('dashboardToken');
			window.location.href = '/login';
		} else {
			// Update UI with username
			console.log('Token verified, user:', data.username);
			const usernameElement = document.querySelector('.username');
			if (usernameElement) {
				usernameElement.textContent = data.username || 'User';
			} else {
				console.warn('Username element not found in DOM');
			}
			// Now that verification is complete, connect to WebSocket
			setTimeout(() => {
				console.log('Starting WebSocket connection');
				connectWebSocket();
			}, 100);
		}
	})
	.catch(err => {
		console.error('Token verification error:', err);
		// Network error - log but don't redirect immediately
		// This prevents redirect loops on network issues
	});
	
	return true;
}

// Get token from localStorage or cookie
function getAuthToken() {
	let token = localStorage.getItem('dashboardToken');
	if (!token) {
		token = getCookie('dashboardToken');
	}
	console.log('getAuthToken called, token available:', !!token);
	return token;
}

// Logout function
function logout() {
	deleteCookie('dashboardToken');
	deleteCookie('gmLevel');
	localStorage.removeItem('dashboardToken');
	window.location.href = '/login';
}

function connectWebSocket() {
	const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
	const token = getAuthToken();
	if (!token) {
		console.error('No token available for WebSocket connection');
		window.location.href = '/login';
		return;
	}
	
	console.log(`WebSocket connection attempt ${reconnectAttempts + 1}/${maxReconnectAttempts}`);
	
	// Connect to WebSocket without token in URL (token is in cookies)
	const wsUrl = `${protocol}//${window.location.host}/ws`;
	console.log(`Connecting to WebSocket: ${wsUrl}`);
	
	try {
		ws = new WebSocket(wsUrl);
		
		ws.onopen = () => {
			console.log('WebSocket connected');
			reconnectAttempts = 0;
			
			// Subscribe to dashboard updates
			ws.send(JSON.stringify({
				event: 'subscribe',
				subscription: 'dashboard_update'
			}));
			
			document.getElementById('connection-status')?.remove();
		};
		
		ws.onmessage = (event) => {
			try {
				const data = JSON.parse(event.data);
				
				// Handle subscription confirmation
				if (data.subscribed) {
					console.log('Subscribed to:', data.subscribed);
					return;
				}
				
				// Handle dashboard updates
				if (data.event === 'dashboard_update') {
					updateDashboard(data);
				}
			} catch (error) {
				console.error('Error parsing WebSocket message:', error);
			}
		};
		
		ws.onerror = (error) => {
			console.error('WebSocket error:', error);
		};
		
		ws.onclose = () => {
			console.log('WebSocket disconnected');
			ws = null;
			
			// Show connection status
			showConnectionStatus('Disconnected - Attempting to reconnect...');
			
			// Attempt to reconnect with exponential backoff
			if (reconnectAttempts < maxReconnectAttempts) {
				reconnectAttempts++;
				const backoffDelay = reconnectDelay * Math.pow(2, reconnectAttempts - 1);
				console.log(`Reconnecting in ${backoffDelay}ms (attempt ${reconnectAttempts}/${maxReconnectAttempts})`);
				setTimeout(connectWebSocket, backoffDelay);
			} else {
				console.error('Max reconnection attempts reached');
				showConnectionStatus('Connection lost - Reload page to reconnect');
			}
		};
	} catch (error) {
		console.error('Failed to create WebSocket:', error);
		showConnectionStatus('Failed to connect - Reload page to retry');
	}
}

function showConnectionStatus(message) {
	let statusEl = document.getElementById('connection-status');
	if (!statusEl) {
		statusEl = document.createElement('div');
		statusEl.id = 'connection-status';
		statusEl.style.cssText = 'position: fixed; top: 10px; right: 10px; background: #f44336; color: white; padding: 10px 20px; border-radius: 4px; z-index: 1000;';
		document.body.appendChild(statusEl);
	}
	statusEl.textContent = message;
}

function updateDashboard(data) {
	// Update server status
	if (data.auth) {
		document.getElementById('auth-status').textContent = data.auth.online ? 'Online' : 'Offline';
		document.getElementById('auth-status').className = 'status ' + (data.auth.online ? 'online' : 'offline');
	}
	
	if (data.chat) {
		document.getElementById('chat-status').textContent = data.chat.online ? 'Online' : 'Offline';
		document.getElementById('chat-status').className = 'status ' + (data.chat.online ? 'online' : 'offline');
	}
	
	// Update world instances
	if (data.worlds) {
		document.getElementById('world-count').textContent = data.worlds.length;
		
		const worldList = document.getElementById('world-list');
		if (data.worlds.length === 0) {
			worldList.innerHTML = '<div class="loading">No active world instances</div>';
		} else {
			worldList.innerHTML = data.worlds.map(world => `
				<div class="world-item">
					<h3>Zone ${world.mapID} - Instance ${world.instanceID}</h3>
					<div class="world-detail">Clone ID: ${world.cloneID}</div>
					<div class="world-detail">Players: ${world.players}</div>
					<div class="world-detail">Type: ${world.isPrivate ? 'Private' : 'Public'}</div>
				</div>
			`).join('');
		}
	}
	
	// Update statistics
	if (data.stats) {
		if (data.stats.onlinePlayers !== undefined) {
			document.getElementById('online-players').textContent = data.stats.onlinePlayers;
		}
		if (data.stats.totalAccounts !== undefined) {
			document.getElementById('total-accounts').textContent = data.stats.totalAccounts;
		}
		if (data.stats.totalCharacters !== undefined) {
			document.getElementById('total-characters').textContent = data.stats.totalCharacters;
		}
	}
}

// Connect on page load
connectWebSocket();
