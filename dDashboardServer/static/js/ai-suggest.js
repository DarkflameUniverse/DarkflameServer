/**
 * The AI moderator helper's Suggest button and panel (staff with ai_suggest, when switched on in Settings).
 *
 * Pages add a button with AiSuggest.button(kind, id); clicking it asks the server for a drafted suggestion (a stored
 * one when the case hasn't changed) and shows it with the evidence it cites. "Use this" only fills in the page's own
 * dialog (the reject/act dialog, the flag review, or the account page's note, mute, ban or strike form): staff still
 * read it and submit it themselves. Nothing is ever applied from here, and nothing reaches players unless staff send it.
 */
(function () {
	'use strict';

	var ACTIONS = {
		dismiss: 'Dismiss', note: 'Add a note', warn: 'Warn', strike: 'Give a strike', mute: 'Mute', ban: 'Ban',
		approve: 'Approve', reject_name: 'Reject the name'
	};
	var CONFIDENCE = { low: 'secondary', medium: 'warning', high: 'success' };
	// Actions done from the account page
	var ACCOUNT_ACTIONS = ['note', 'warn', 'strike', 'mute', 'ban'];
	var PREFILL_KEY = 'aiPrefill';

	function can() { return window.DASH && DASH.can && DASH.can('ai_suggest'); }

	function actionText(s) {
		var text = ACTIONS[s.action] || s.action;
		if (s.days) text += ' for ' + s.days + ' day' + (s.days === 1 ? '' : 's');
		if (s.strike && s.action !== 'strike') text += ', with a strike';
		return text;
	}

	function clip(text, n) { text = String(text || ''); return text.length > n ? text.slice(0, n - 1) + '…' : text; }

	// ---- Status: hide the buttons while the helper is off ----

	var statusPromise = null;
	function status() {
		if (!statusPromise) statusPromise = api.get('/api/ai/status').then(function (d) { return d.success ? d : null; }).catch(function () { return null; });
		return statusPromise;
	}

	function budgetText(b) {
		if (!b) return '';
		return b.remaining_today + ' of ' + b.per_day + ' suggestions left today (' + b.per_minute + ' a minute)';
	}

	// ---- The panel ----

	var el = null, modal = null, current = null;

	function build() {
		el = document.createElement('div');
		el.className = 'modal fade';
		el.dataset.navKeep = ''; // shared by every page: kept when nav.js swaps pages
		el.tabIndex = -1;
		el.setAttribute('aria-labelledby', 'aiSuggestTitle');
		el.innerHTML =
			'<div class="modal-dialog modal-lg modal-dialog-scrollable"><div class="modal-content">' +
			'<div class="modal-header"><h5 class="modal-title" id="aiSuggestTitle">AI suggestion <span class="badge text-bg-secondary align-middle">draft for staff</span></h5>' +
			'<button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button></div>' +
			'<div class="modal-body" id="aiSuggestBody"></div>' +
			'<div class="modal-footer justify-content-between"><span class="small text-body-secondary" id="aiSuggestBudget"></span><div class="d-flex flex-wrap gap-2">' +
			'<button type="button" class="btn btn-outline-secondary d-none" id="aiSuggestAgain" title="Ask again, even though a stored suggestion fits (uses the budget)">Ask again</button>' +
			'<button type="button" class="btn btn-outline-primary d-none" id="aiSuggestAccount">Fill in on the account page</button>' +
			'<button type="button" class="btn btn-primary d-none" id="aiSuggestUse">Use this</button>' +
			'<button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Close</button></div></div>' +
			'</div></div>';
		document.body.appendChild(el);
		modal = new bootstrap.Modal(el);
		el.querySelector('#aiSuggestAgain').addEventListener('click', function () { if (current) ask(current.kind, current.id, current.button, true); });
		el.querySelector('#aiSuggestUse').addEventListener('click', function () {
			if (!current || !current.data) return;
			var c = current;
			modal.hide();
			use(c);
		});
		el.querySelector('#aiSuggestAccount').addEventListener('click', function () {
			if (current && current.data) fillOnAccountPage(current.data);
		});
	}

	function setButtons(data) {
		var s = data && data.suggestion;
		el.querySelector('#aiSuggestAgain').classList.toggle('d-none', !data);
		el.querySelector('#aiSuggestUse').classList.toggle('d-none', !s || !usable(current.kind, s));
		el.querySelector('#aiSuggestAccount').classList.toggle('d-none', !s || !data.account_id || ACCOUNT_ACTIONS.indexOf(s.action) === -1 || !DASH.can('accounts_view'));
	}

	function showMessage(html, type) {
		el.querySelector('#aiSuggestBody').innerHTML = '<div class="alert alert-' + (type || 'secondary') + ' mb-0">' + html + '</div>';
	}

	// One line of the case file for a ref the answer cites
	function evidenceLine(data, ref) {
		var c = data['case'] || {};
		if (ref === 'ITEM') return 'The ' + (c.kind || 'item').replace(/_/g, ' ') + ' itself';
		var lists = { M: c.chat || [], S: (c.account_history || {}).strikes || [], H: (c.account_history || {}).earlier_moderation || [] };
		var found = (lists[ref.charAt(0)] || []).filter(function (x) { return x.ref === ref; })[0];
		if (!found) return '';
		if (ref.charAt(0) === 'M') return found.when + ' · ' + found.channel + ' · ' + found.from + (found.to ? ' → ' + found.to : '') + ': ' + found.text + (found.stopped_by_filter ? ' (stopped by the filter)' : '');
		if (ref.charAt(0) === 'S') return found.when + ' · strike for ' + found['for'] + (found.about ? ' (' + found.about + ')' : '') + (found.reason ? ': ' + found.reason : '') + (found.still_counts ? '' : ' (no longer counts)');
		return found.when + ' · ' + found.kind + ': ' + found.text;
	}

	function render(data) {
		current.data = data;
		var s = data.suggestion;
		var meta = (data.cached ? 'Stored suggestion from ' + fmt.unix(data.created_at) + (data.requested_by ? ', asked by ' + data.requested_by : '') + ' (the case hasn\'t changed since; free)'
			: 'Just asked') + ' · ' + (data.model || '') + ' · ' + (data.input_tokens + data.output_tokens) + ' tokens';
		var evidence = (s.evidence || []).map(function (ref) {
			return '<li><span class="badge text-bg-light border me-1">' + esc(ref) + '</span>' + esc(evidenceLine(data, ref)) + '</li>';
		}).join('');
		el.querySelector('#aiSuggestBody').innerHTML =
			'<p class="small text-body-secondary">Written by an AI model from the case below. It can be wrong or miss context: check the evidence. ' +
			'Nothing happens until you do it yourself.</p>' +
			'<div class="d-flex flex-wrap align-items-center gap-2 mb-3"><span class="fs-5 fw-semibold">' + esc(actionText(s)) + '</span>' +
			fmt.badge(s.confidence + ' confidence', CONFIDENCE[s.confidence] || 'secondary') + '</div>' +
			(s.player_reason ? '<h6 class="small text-uppercase text-body-secondary mb-1">Reason for the player</h6>' +
				'<blockquote class="border-start border-3 ps-3 mb-3">' + esc(s.player_reason) + '</blockquote>' : '') +
			'<h6 class="small text-uppercase text-body-secondary mb-1">Why</h6><p style="white-space: pre-wrap">' + esc(s.staff_explanation) + '</p>' +
			(evidence ? '<h6 class="small text-uppercase text-body-secondary mb-1">Evidence it cites</h6><ul class="list-unstyled small mb-3">' + evidence + '</ul>' : '') +
			'<div class="small text-body-secondary">' + esc(meta) + '</div>';
		el.querySelector('#aiSuggestBudget').textContent = budgetText(data.budget);
		setButtons(data);
	}

	function ask(kind, id, button, refresh) {
		if (!el) build();
		current = { kind: kind, id: id, button: button, data: null };
		setButtons(null);
		el.querySelector('#aiSuggestBudget').textContent = '';
		showMessage('<span class="spinner-border spinner-border-sm me-2" role="status"></span>Asking the AI helper&hellip; this can take a little while.');
		modal.show();
		var mine = current;
		api.post('/api/ai/suggest', { kind: kind, id: String(id), refresh: !!refresh }).then(function (d) {
			if (mine !== current) return null;
			if (!d.success) { showMessage(esc(d.error || 'Request failed'), 'warning'); el.querySelector('#aiSuggestAgain').classList.add('d-none'); return null; }
			if (d.budget) el.querySelector('#aiSuggestBudget').textContent = budgetText(d.budget);
			if (d.suggestion) return d;
			return Live.waitForResult(d.requestId).then(function (r) {
				if (!r.success) {
					showMessage(esc(r.message || 'The helper failed'), 'warning');
					el.querySelector('#aiSuggestAgain').classList.remove('d-none');
					return null;
				}
				return r.data;
			});
		}).then(function (data) {
			if (data && mine === current) render(data);
		}).catch(function () { if (mine === current) showMessage('Could not reach the dashboard.', 'danger'); });
	}

	// ---- "Use this": fill in the page's own dialog; staff submit it ----

	function row(c) { return c.button ? c.button.closest('tr') || document : document; }

	function highlight(button) {
		if (!button) return;
		button.classList.add('ai-suggested');
		button.scrollIntoView({ block: 'center' });
		button.focus();
		setTimeout(function () { button.classList.remove('ai-suggested'); }, 4000);
	}

	function usable(kind, s) {
		if (kind === 'name' || kind === 'pet_name' || kind === 'player_report') return true;
		if (kind === 'economy_flag') return !!document.querySelector('[data-review]') && DASH.can('reports_review_flags');
		return false; // chat: done on the account page
	}

	function reportReason(s) {
		return clip(actionText(s) + (s.player_reason ? ': ' + s.player_reason : ''), 300);
	}

	function use(c) {
		var s = c.data.suggestion, scope = row(c), target;
		if (c.kind === 'name' || c.kind === 'pet_name') {
			if (s.action === 'approve') {
				target = scope.querySelector('[data-action="approve"]');
				highlight(target);
				toast('Click Approve to apply it', 'info');
				return;
			}
			target = scope.querySelector('[data-action="reject"]');
			if (!target) return toast('The name is no longer waiting', 'warning');
			window.Decide.next = { reason: s.player_reason, strike: s.strike };
			target.click();
		} else if (c.kind === 'player_report') {
			target = s.action === 'dismiss' ? scope.querySelector('[data-dismiss-report]') : scope.querySelector('[data-act]');
			if (!target) return toast('The report is no longer open', 'warning');
			window.Decide.next = s.action === 'dismiss' ? { reason: clip(s.staff_explanation, 300) } : { reason: reportReason(s), strike: s.strike || s.action === 'strike' };
			target.click();
			if (s.action !== 'dismiss' && s.action !== 'note' && c.data.account_id) {
				toast('Closing the report doesn\'t ' + actionText(s).toLowerCase() + ': use "Fill in on the account page" for that', 'info');
			}
		} else if (c.kind === 'economy_flag') {
			target = scope.querySelector('[data-review]');
			if (!target) return;
			target.click();
			var note = document.getElementById('flagNote');
			if (note) note.value = clip(actionText(s) + ': ' + s.staff_explanation, 1000);
			highlight(document.querySelector('[data-flag-status="' + (s.action === 'dismiss' ? 'dismissed' : 'actioned') + '"]'));
		}
	}

	function fillOnAccountPage(data) {
		var s = data.suggestion;
		var prefill = { account: String(data.account_id), action: s.action, days: s.days, strike: s.strike,
			reason: s.action === 'note' ? clip(s.staff_explanation, 300) : s.player_reason, at: Date.now() };
		try { sessionStorage.setItem(PREFILL_KEY, JSON.stringify(prefill)); } catch (e) { return toast('Could not pass the suggestion on (browser storage is off)', 'warning'); }
		goTo('/accounts/' + encodeURIComponent(data.account_id));
	}

	// On an account page: fill in the form a suggestion was passed to. Nothing is submitted.
	function applyAccountPrefill() {
		var match = location.pathname.match(/^\/accounts\/(\d+)/);
		if (!match) return;
		var prefill = null;
		try {
			prefill = JSON.parse(sessionStorage.getItem(PREFILL_KEY) || 'null');
			sessionStorage.removeItem(PREFILL_KEY);
		} catch (e) { return; }
		if (!prefill || prefill.account !== match[1] || Date.now() - prefill.at > 5 * 60 * 1000) return;

		function pick(select, days) {
			if (!select) return;
			var value = String(days);
			if (!Array.prototype.some.call(select.options, function (o) { return o.value === value; })) {
				var option = document.createElement('option');
				option.value = value;
				option.textContent = days + ' days (suggested)';
				select.appendChild(option);
			}
			select.value = value;
		}
		function showModal(id) {
			var m = document.getElementById(id);
			if (m) bootstrap.Modal.getOrCreateInstance(m).show();
			return !!m;
		}

		var done = false;
		if (prefill.action === 'note' || prefill.action === 'warn') {
			var kind = document.getElementById('noteKind'), text = document.getElementById('noteText');
			if (kind && text) {
				kind.value = prefill.action === 'warn' ? 'warning' : 'note';
				kind.dispatchEvent(new Event('change', { bubbles: true }));
				text.value = prefill.reason || '';
				text.scrollIntoView({ block: 'center' });
				text.focus();
				done = true;
			}
		} else if (prefill.action === 'mute') {
			pick(document.getElementById('muteDuration'), prefill.days);
			var muteReason = document.getElementById('muteReason');
			if (muteReason) muteReason.value = prefill.reason || '';
			done = showModal('muteModal');
		} else if (prefill.action === 'ban') {
			pick(document.getElementById('banDays'), prefill.days);
			var banReason = document.getElementById('banReason');
			if (banReason) banReason.value = prefill.reason || '';
			done = showModal('banModal');
		} else if (prefill.action === 'strike') {
			var give = document.getElementById('giveStrike');
			if (give) { window.Decide.next = { reason: prefill.reason }; give.click(); done = true; }
		}
		if (!done) return toast('This account page has no form for that, or you can\'t use it', 'warning');
		toast('Filled in from the AI suggestion: check it, then apply it yourself' +
			(prefill.strike && prefill.action !== 'strike' ? '. It also suggests a strike (Strikes card)' : ''), 'info');
	}

	// ---- Chat log: a button for the character typed in the filter ----

	function addPlayerChatButton() {
		var input = document.getElementById('chatCharacter');
		if (!input || document.getElementById('aiPlayerChat')) return;
		var button = document.createElement('button');
		button.type = 'button';
		button.id = 'aiPlayerChat';
		// Beside the label rather than under the input, so the filter row's inputs stay lined up
		button.className = 'btn btn-link btn-sm p-0 float-end lh-sm align-baseline ai-suggest-btn';
		button.textContent = 'Suggest for their recent chat';
		button.addEventListener('click', function () {
			if (!input.value.trim()) { input.focus(); return toast('Type a character first', 'info'); }
			ask('player_chat', input.value.trim(), null, false);
		});
		input.parentNode.insertBefore(button, input);
	}

	// ---- Wiring ----

	var style = document.createElement('style');
	style.textContent = 'body.ai-helper-off .ai-suggest-btn { display: none !important; } .ai-suggested { box-shadow: 0 0 0 .25rem rgba(var(--bs-info-rgb), .6) !important; }';
	document.head.appendChild(style);
	// Suggest buttons stay hidden until we know the helper is on, so pages don't show buttons that can't work
	if (can()) document.body.classList.add('ai-helper-off');

	window.AiSuggest = {
		// HTML for a Suggest button (empty without ai_suggest)
		button: function (kind, id, extraClass) {
			if (!can() || !id || id === '0') return '';
			return '<button type="button" class="btn btn-sm btn-outline-info text-nowrap ai-suggest-btn' + (extraClass ? ' ' + extraClass : '') + '" data-ai-suggest="' + esc(kind) +
				'" data-ai-id="' + esc(id) + '" title="Draft a suggestion with the AI helper. Staff only; nothing is applied">Suggest</button>';
		},
		ask: ask
	};

	document.addEventListener('click', function (e) {
		var button = e.target.closest('[data-ai-suggest]');
		if (!button) return;
		e.preventDefault();
		ask(button.getAttribute('data-ai-suggest'), button.getAttribute('data-ai-id'), button, false);
	});

	function start() {
		applyAccountPrefill();
		if (!can()) return;
		addPlayerChatButton();
		status().then(function (d) {
			if (d && d.enabled) document.body.classList.remove('ai-helper-off');
			var budget = document.getElementById('aiBudget');
			if (budget && d && d.enabled) {
				budget.textContent = d.problem ? 'AI helper: ' + d.problem : 'AI helper: ' + budgetText(d.budget);
				budget.classList.remove('d-none');
			}
		});
	}
	// After the page's own scripts have set up their dialogs, and again for every page nav.js swaps in
	if (document.readyState === 'complete') setTimeout(start, 0);
	else window.addEventListener('load', function () { setTimeout(start, 0); });
	document.addEventListener('dash:page', function () { setTimeout(start, 0); });
})();
