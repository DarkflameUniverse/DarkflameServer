/**
 * Generic Real-time WebSocket Updates for Dashboard Tables
 * Provides reactive, non-blocking updates for all admin tables
 */

const realtimeManager = {
	tables: {},
	currentEntityId: null,
	wsReady: false,

	/**
	 * Initialize real-time updates for a DataTable
	 * @param {string} entityType - Type of entity (account, character, property, etc.)
	 * @param {jQuery} tableElement - jQuery DataTable element
	 */
	RegisterTable(entityType, tableElement) {
		if (!tableElement || !tableElement.DataTable) {
			console.warn(`Invalid table element for entity type: ${entityType}`);
			return;
		}

		this.tables[entityType] = {
			element: tableElement,
			table: tableElement.DataTable(),
			lastUpdate: Date.now()
		};

		console.log(`Registered real-time table for entity type: ${entityType}`);
	},

	/**
	 * Initialize WebSocket listeners for all entity types
	 */
	Initialize() {
		if (!ws || ws.readyState !== WebSocket.OPEN) {
			console.warn('WebSocket not ready for realtime initialization');
			return;
		}

		this.wsReady = true;
		console.log('Initialized real-time WebSocket listeners');
		this.SubscribeToAll();
	},

	/**
	 * Subscribe to all registered entity types
	 */
	SubscribeToAll() {
		const subscriptions = [
			'account_update',
			'accounts_table_update',
			'account_list_changed',
			'character_update',
			'characters_table_update',
			'character_list_changed',
			'property_update',
			'properties_table_update',
			'property_list_changed',
			'play_key_update',
			'play_keys_table_update',
			'play_key_list_changed',
			'bug_report_update',
			'bug_reports_table_update'
		];

		for (const subscription of subscriptions) {
			if (!ws || ws.readyState !== WebSocket.OPEN) break;

			ws.send(JSON.stringify({
				event: 'subscribe',
				subscription: subscription
			}));
		}

		console.log(`Subscribed to ${subscriptions.length} real-time events`);
	},

	/**
	 * Handle incoming WebSocket messages
	 */
	HandleMessage(data) {
		if (!data.event) return;

		// Route to appropriate handler based on event type
		if (data.event.includes('account')) {
			this.HandleAccountEvent(data);
		} else if (data.event.includes('character')) {
			this.HandleCharacterEvent(data);
		} else if (data.event.includes('property')) {
			this.HandlePropertyEvent(data);
		} else if (data.event.includes('play_key')) {
			this.HandlePlayKeyEvent(data);
		} else if (data.event.includes('bug_report')) {
			this.HandleBugReportEvent(data);
		}
	},

	/**
	 * Handle account-related events
	 */
	HandleAccountEvent(data) {
		if (data.event === 'account_update') {
			this.UpdateRow('account', data.accountId || data.id, data);
			// Also update detail panel if viewing this account
			this.UpdateDetailPanel('account', data);
		} else if (data.event === 'accounts_table_update') {
			this.ReloadTable('account');
		} else if (data.event === 'account_list_changed') {
			this.UpdateListCount('account', data.totalAccounts);
			this.ReloadTable('account', true);
		}
	},

	/**
	 * Handle character-related events
	 */
	HandleCharacterEvent(data) {
		if (data.event === 'character_update') {
			this.UpdateRow('character', data.characterId || data.id, data);
		} else if (data.event === 'characters_table_update') {
			this.ReloadTable('character');
		} else if (data.event === 'character_list_changed') {
			this.UpdateListCount('character', data.totalCharacters);
			this.ReloadTable('character', true);
		}
	},

	/**
	 * Handle property-related events
	 */
	HandlePropertyEvent(data) {
		if (data.event === 'property_update') {
			this.UpdateRow('property', data.propertyId || data.id, data);
		} else if (data.event === 'properties_table_update') {
			this.ReloadTable('property');
		} else if (data.event === 'property_list_changed') {
			this.UpdateListCount('property', data.totalProperties);
			this.ReloadTable('property', true);
		}
	},

	/**
	 * Handle play key-related events
	 */
	HandlePlayKeyEvent(data) {
		if (data.event === 'play_key_update') {
			this.UpdateRow('play_key', data.playKeyId || data.id, data);
		} else if (data.event === 'play_keys_table_update') {
			this.ReloadTable('play_key');
		} else if (data.event === 'play_key_list_changed') {
			this.UpdateListCount('play_key', data.totalPlayKeys);
			this.ReloadTable('play_key', true);
		}
	},

	/**
	 * Handle bug report-related events
	 */
	HandleBugReportEvent(data) {
		if (data.event === 'bug_report_update') {
			this.UpdateRow('bug_report', data.bugReportId || data.id, data);
		} else if (data.event === 'bug_reports_table_update') {
			this.ReloadTable('bug_report');
		}
	},

	/**
	 * Update a single row in a DataTable
	 */
	UpdateRow(entityType, entityId, data) {
		if (!this.tables[entityType]) {
			console.debug(`No table registered for entity type: ${entityType}`);
			return;
		}

		try {
			const table = this.tables[entityType].table;
			const rows = table.rows().nodes();
			
			// Find and invalidate the matching row
			let found = false;
			for (let row of rows) {
				const rowData = table.row(row).data();
				if (rowData && (rowData.id === entityId || rowData.accountId === entityId || 
					rowData.characterId === entityId || rowData.propertyId === entityId || 
					rowData.playKeyId === entityId)) {
					table.row(row).invalidate().draw(false);
					found = true;
					break;
				}
			}

			if (found) {
				this.ShowToast('Updated', `${entityType} data has been refreshed`, 'info');
			}
		} catch (e) {
			console.debug(`Error updating row for ${entityType}:`, e);
		}
	},

	/**
	 * Reload a table without page change
	 */
	ReloadTable(entityType, resetPage = false) {
		if (!this.tables[entityType]) {
			console.debug(`No table registered for entity type: ${entityType}`);
			return;
		}

		try {
			const table = this.tables[entityType].table;
			
			if (resetPage) {
				table.page(0).ajax.reload();
			} else {
				table.ajax.reload(null, false);
			}

			this.ShowToast('Updated', `${entityType} list has been refreshed`, 'info');
		} catch (e) {
			console.error(`Error reloading table for ${entityType}:`, e);
		}
	},

	/**
	 * Update list count display
	 */
	UpdateListCount(entityType, count) {
		try {
			const selector = `.${entityType}-count`;
			const countEl = document.querySelector(selector);
			if (countEl) {
				countEl.textContent = count;
			}
		} catch (e) {
			console.debug(`Error updating count for ${entityType}:`, e);
		}
	},

	/**
	 * Set current entity being viewed for detail updates
	 */
	SetCurrentEntity(entityType, entityId) {
		this.currentEntity = { type: entityType, id: entityId };
		console.log(`Viewing ${entityType} ID: ${entityId}`);
	},

	/**
	 * Clear current entity
	 */
	ClearCurrentEntity() {
		this.currentEntity = null;
	},

	/**
	 * Update detail panel for current entity
	 */
	UpdateDetailPanel(entityType, data) {
		if (!this.currentEntity || this.currentEntity.type !== entityType || 
			this.currentEntity.id !== (data.id || data.accountId)) {
			return;
		}

		try {
			// Update all elements with data-field attributes
			for (const [key, value] of Object.entries(data)) {
				const selectors = [
					`.detail-${key}`,
					`[data-field="${key}"]`,
					`.${entityType}-${key}`
				];

				for (const selector of selectors) {
					const elements = document.querySelectorAll(selector);
					for (const el of elements) {
						this.UpdateElement(el, key, value);
					}
				}
			}

			this.ShowToast('Updated', `${entityType} data refreshed`, 'info');
		} catch (e) {
			console.error(`Error updating detail panel:`, e);
		}
	},

	/**
	 * Update an individual element with new value
	 */
	UpdateElement(element, fieldName, value) {
		if (!element) return;

		try {
			// Handle different field types
			if (fieldName.includes('banned') || fieldName.includes('banned') || fieldName.includes('locked')) {
				element.textContent = value ? 'Yes' : 'No';
				element.className = `badge ${value ? 'bg-danger' : 'bg-success'}`;
			} else if (fieldName.includes('count') || fieldName.includes('level')) {
				element.textContent = value || '-';
			} else if (fieldName.includes('date') || fieldName.includes('created') || fieldName.includes('updated')) {
				if (value) {
					element.textContent = new Date(value * 1000).toLocaleString();
				}
			} else {
				element.textContent = value || '-';
			}
		} catch (e) {
			console.debug(`Error updating element for field ${fieldName}:`, e);
		}
	},

	/**
	 * Show toast notification
	 */
	ShowToast(title, message, type = 'info') {
		try {
			const toastEl = document.createElement('div');
			toastEl.className = `alert alert-${type === 'error' ? 'danger' : type} alert-dismissible fade show`;
			toastEl.setAttribute('role', 'alert');
			toastEl.style.cssText = 'position: fixed; bottom: 20px; right: 20px; z-index: 9999; min-width: 300px;';

			toastEl.innerHTML = `
				<strong>${title}</strong><br>${message}
				<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
			`;

			document.body.appendChild(toastEl);

			setTimeout(() => {
				toastEl.remove();
			}, 4000);
		} catch (e) {
			console.log(`[${type}] ${title}: ${message}`);
		}
	}
};

// Export to global scope
window.realtimeManager = realtimeManager;
