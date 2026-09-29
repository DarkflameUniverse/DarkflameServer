/**
 * Picking chat messages and flagging them for review, shared by the Chat Log and the chat histories (guild, team and
 * whisper conversations). Each message row has a checkbox (ChatFlagging.checkbox); shift-click picks every message
 * between the last one clicked and this one. The bar (ChatFlagging.bar) shows how many are picked and opens the flag
 * dialog: who the flag is about (one of the senders) and a note. The server keeps a copy of the messages and the chat
 * around them. Messages already flagged link to their flag (ChatFlagging.flagLink).
 */
window.ChatFlagging = (function () {
	'use strict';

	var CHANNEL_NEEDS = { whisper: 'chat_dms', team: 'chat_private', guild: 'chat_private' };

	function flagLink(row) {
		if (!row.flag_id) return '';
		return ' <a class="badge text-bg-warning text-decoration-none" href="/chat_flags#flag=' + esc(row.flag_id) + '" title="In chat flag ' + esc(row.flag_id) + '">Flagged</a>';
	}

	// The message text, or why it can't be shown
	function text(row) {
		if (row.redacted) return '<span class="text-body-secondary fst-italic">Hidden: reading ' + esc(row.channel) + ' chat needs ' + esc(CHANNEL_NEEDS[row.channel] || 'another permission') + '</span>';
		var badges = '';
		if (row.blocked) badges += fmt.badge('Stopped by the filter', 'danger') + ' ';
		else if (row.filtered) badges += fmt.badge('Filter words', 'warning') + ' ';
		return badges + '<span class="' + (row.blocked ? 'text-body-secondary' : '') + '">' + esc(row.message) + '</span>';
	}

	function checkbox(row) {
		if (!DASH.can('chat_flag') || row.redacted || !row.id) return '';
		return '<input class="form-check-input" type="checkbox" data-flag-pick="' + esc(row.id) + '" aria-label="Pick this message to flag">';
	}

	/**
	 * Picking in `container` (an element, or a selector), with the bar in `barEl`. onFlagged(id) runs after a flag
	 * is made.
	 */
	function create(container, barEl, onFlagged) {
		var root = typeof container === 'string' ? document.querySelector(container) : container;
		var bar = typeof barEl === 'string' ? document.querySelector(barEl) : barEl;
		var rows = {};      // id -> message, for every message shown
		var picked = {};    // id -> true
		var lastClicked = null;

		function count() { return Object.keys(picked).length; }

		function renderBar() {
			if (!bar) return;
			var n = count();
			bar.classList.toggle('d-none', !DASH.can('chat_flag'));
			bar.innerHTML = '<span class="small text-body-secondary">' + (n ? n + ' message' + (n === 1 ? '' : 's') + ' picked' :
				'Tick messages to flag them for review (shift-click picks a range).') + '</span>' +
				(n ? ' <button type="button" class="btn btn-sm btn-warning" data-flag-open>Flag&hellip;</button> <button type="button" class="btn btn-sm btn-outline-secondary" data-flag-clear>Clear</button>' : '');
		}

		function sync() {
			root.querySelectorAll('[data-flag-pick]').forEach(function (box) { box.checked = !!picked[box.getAttribute('data-flag-pick')]; });
			renderBar();
		}

		function clear() { picked = {}; lastClicked = null; sync(); }

		root.addEventListener('click', function (e) {
			var box = e.target.closest('[data-flag-pick]');
			if (!box) return;
			var id = box.getAttribute('data-flag-pick');
			var on = box.checked;
			if (e.shiftKey && lastClicked !== null) {
				var boxes = Array.prototype.slice.call(root.querySelectorAll('[data-flag-pick]'));
				var ids = boxes.map(function (b) { return b.getAttribute('data-flag-pick'); });
				var a = ids.indexOf(lastClicked), b = ids.indexOf(id);
				if (a !== -1 && b !== -1) {
					for (var i = Math.min(a, b); i <= Math.max(a, b); i++) {
						if (on) picked[ids[i]] = true; else delete picked[ids[i]];
					}
				}
			} else if (on) picked[id] = true;
			else delete picked[id];
			lastClicked = id;
			sync();
		});

		if (bar) bar.addEventListener('click', function (e) {
			if (e.target.closest('[data-flag-clear]')) return clear();
			if (e.target.closest('[data-flag-open]')) openDialog();
		});

		function openDialog() {
			var ids = Object.keys(picked);
			var messages = ids.map(function (id) { return rows[id]; }).filter(Boolean).sort(function (a, b) { return a.id - b.id; });
			var senders = [];
			messages.forEach(function (m) {
				if (m.sender_id !== '0' && !senders.some(function (s) { return s.id === m.sender_id; })) senders.push({ id: m.sender_id, name: m.sender_name });
			});
			var modalEl = document.createElement('div');
			modalEl.className = 'modal fade';
			modalEl.tabIndex = -1;
			modalEl.innerHTML = '<div class="modal-dialog modal-lg"><form class="modal-content">' +
				'<div class="modal-header"><h5 class="modal-title">Flag ' + ids.length + ' message' + (ids.length === 1 ? '' : 's') + '</h5><button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button></div>' +
				'<div class="modal-body">' +
				'<div class="border rounded p-2 mb-3 small" style="max-height: 14rem; overflow-y: auto">' + messages.map(function (m) {
					return '<div><span class="text-body-secondary">' + esc(fmt.unix(m.time)) + '</span> <strong>' + esc(m.sender_name) + '</strong>' +
						(m.recipient_name ? ' &rarr; ' + esc(m.recipient_name) : '') + ': ' + esc(m.message) + '</div>';
				}).join('') + '</div>' +
				(senders.length ? '<div class="mb-3"><label class="form-label" for="flagAbout">About</label><select class="form-select" id="flagAbout">' +
					senders.map(function (s) { return '<option value="' + esc(s.id) + '">' + esc(s.name) + '</option>'; }).join('') + '</select></div>' : '') +
				'<div><label class="form-label" for="flagNote">Note</label><textarea class="form-control" id="flagNote" rows="3" maxlength="2000" placeholder="What is wrong here, for whoever reviews it"></textarea>' +
				'<div class="form-text">The flag keeps a copy of these messages and the chat around them, even after the chat log is pruned. The messages must be from one conversation.</div></div>' +
				'</div>' +
				'<div class="modal-footer"><button type="button" class="btn btn-outline-secondary" data-bs-dismiss="modal">Cancel</button><button type="submit" class="btn btn-warning">Flag</button></div>' +
				'</form></div>';
			document.body.appendChild(modalEl);
			var modal = new bootstrap.Modal(modalEl);
			modalEl.addEventListener('hidden.bs.modal', function () { modal.dispose(); modalEl.remove(); });
			modalEl.querySelector('form').addEventListener('submit', function (e) {
				e.preventDefault();
				var about = modalEl.querySelector('#flagAbout');
				api.action('/api/chat/flags', { message_ids: ids, character: about ? about.value : '', note: modalEl.querySelector('#flagNote').value }, 'Flagged for review').then(function (d) {
					modal.hide();
					clear();
					if (onFlagged) onFlagged(d.id);
				}).catch(function () {});
			});
			modal.show();
		}

		renderBar();
		return {
			// Remember the messages shown (for the dialog); call before or after rendering them
			remember: function (messages) { messages.forEach(function (m) { rows[m.id] = m; }); },
			// After the rows were drawn again: tick what's picked
			sync: sync,
			clear: clear
		};
	}

	return { create: create, checkbox: checkbox, flagLink: flagLink, text: text };
})();
