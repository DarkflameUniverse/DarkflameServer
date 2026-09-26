/**
 * The Chat Filter page: words staff block or allow on top of the filter's files, and for a word, which recent chat it
 * would have stopped (or which messages the filter stopped that contain it), and where it stands in the files. The files'
 * allowed words can be browsed and copied into the Allowed list. Every change is applied in running worlds.
 */
(function () {
	'use strict';

	var input = document.getElementById('wordInput');
	var lastSubmitter = null;

	function item(w) {
		return '<li class="list-group-item d-flex justify-content-between align-items-center gap-2">' +
			'<div><strong>' + esc(w.word) + '</strong><div class="small text-body-secondary">' + esc(w.added_by) + ', ' + esc(fmt.unix(w.added_at)) + '</div></div>' +
			'<div class="d-flex gap-1"><button type="button" class="btn btn-sm btn-outline-secondary" data-pick="' + esc(w.word) + '">Check</button>' +
			'<button type="button" class="btn btn-sm btn-outline-danger" data-remove="' + esc(w.word) + '">Remove</button></div></li>';
	}

	function load() {
		api.get('/api/chat_filter').then(function (d) {
			if (!d.success) return;
			var blocked = d.words.filter(function (w) { return !w.allowed; }), allowed = d.words.filter(function (w) { return w.allowed; });
			document.getElementById('blockedCount').textContent = blocked.length;
			document.getElementById('allowedCount').textContent = allowed.length;
			document.getElementById('blockedList').innerHTML = blocked.map(item).join('') || '<li class="list-group-item text-body-secondary">None added.</li>';
			document.getElementById('allowedList').innerHTML = allowed.map(item).join('') || '<li class="list-group-item text-body-secondary">None added.</li>';
		}).catch(function () {});
	}

	// The words of the filter's files (chatplus_en_us.txt; blocklist.dcf holds only hashes, so only its size is known)
	var fileSearch = document.getElementById('fileSearch'), fileWords = [], fileStart = 0, fileTimer = null;
	function loadFiles(more) {
		fileStart = more ? fileStart + fileWords.length : 0;
		api.get('/api/chat_filter/files?search=' + encodeURIComponent(fileSearch.value.trim()) + '&start=' + fileStart).then(function (d) {
			if (!d.success) return;
			fileWords = more ? fileWords.concat(d.words) : d.words;
			fileStart = more ? fileStart : 0;
			document.getElementById('fileCount').textContent = d.allowFileFound ? d.allowTotal : '';
			document.getElementById('fileHelp').textContent = (d.allowFileFound
				? d.allowTotal + ' words are allowed by ' + d.allowFile + ' (from the client); ' + d.onDashboard + ' of them are on the lists above too, which win. '
				: 'Could not read ' + d.allowFile + ' from the client: set client_location. ') +
				(d.blockFileFound ? d.blockFile + ' blocks ' + d.blockTotal + ' more words, stored only as hashes, so they can\'t be listed: check a word above to see if it is one.'
					: 'There is no ' + d.blockFile + ' next to the servers.');
			document.getElementById('fileWords').innerHTML = fileWords.map(function (w) {
				var cls = w.dashboard === 'blocked' ? 'text-bg-danger' : w.dashboard === 'allowed' ? 'text-bg-success' : 'text-bg-secondary';
				var title = w.dashboard ? (w.dashboard === 'blocked' ? 'Blocked on the list above, whatever the file says' : 'Also on the Allowed list above') : 'Only in the file';
				return '<button type="button" class="badge border-0 ' + cls + '" data-pick="' + esc(w.word) + '" title="' + esc(title) + '">' + esc(w.word) + '</button>';
			}).join('') || '<span class="text-body-secondary small">No words match.</span>';
			var more = document.getElementById('fileMore');
			more.classList.toggle('d-none', fileWords.length >= d.matched);
			more.textContent = 'Show more (' + (d.matched - fileWords.length) + ' left)';
		}).catch(function () {});
	}

	// Where a word stands: the files and the lists here
	function lookup(word) {
		var el = document.getElementById('lookupResult');
		api.get('/api/chat_filter/lookup?word=' + encodeURIComponent(word)).then(function (d) {
			if (!d.success) return;
			var parts = [];
			parts.push(d.inAllowFile ? 'allowed by the file' : 'not in the allowed words file');
			if (d.inBlockFile) parts.push('blocked by the blocked words file');
			if (d.dashboard) parts.push((d.dashboard === 'blocked' ? 'blocked' : 'allowed') + ' on the list here (this wins)');
			el.innerHTML = '<strong>' + esc(d.word) + '</strong>: ' + esc(parts.join('; ')) + '.';
			el.classList.remove('d-none');
		}).catch(function () {});
	}

	function check(word, allowed) {
		lookup(word);
		api.get('/api/chat_filter/check?word=' + encodeURIComponent(word) + '&allowed=' + (allowed ? 1 : 0)).then(function (d) {
			if (!d.success) { toast(d.error || 'Check failed', 'danger'); return; }
			document.getElementById('checkResult').classList.remove('d-none');
			document.getElementById('checkTitle').textContent = d.allowed
				? d.messages.length + ' stopped message(s) with "' + d.word + '"'
				: d.messages.length + ' message(s) blocking "' + d.word + '" would have stopped';
			document.getElementById('checkHelp').textContent = 'From the newest ' + d.limit + ' messages containing the text' +
				(d.allowed ? '. Allowing the word lets these through only if their other words are allowed too.' : ' that players saw.');
			document.getElementById('checkRows').innerHTML = d.messages.map(function (m) {
				return '<tr><td class="small text-nowrap">' + esc(fmt.unix(m.time)) + '</td><td class="small">' + (m.zone_id ? fmt.zone(m.zone_id, m.zone_name) : esc(m.channel)) + '</td>' +
					'<td>' + fmt.character(m.sender_id, m.sender_name) + '</td><td>' + esc(m.message) + '</td>' +
					'<td>' + (m.account_id ? '<a class="btn btn-sm btn-outline-secondary" href="/accounts/' + esc(m.account_id) + '">Account</a>' : '') + '</td></tr>';
			}).join('') || '<tr><td colspan="5" class="text-body-secondary">None.</td></tr>';
		}).catch(function () {});
	}

	document.getElementById('wordForm').addEventListener('click', function (e) {
		var button = e.target.closest('button');
		if (!button) return;
		if (button.type === 'submit') { lastSubmitter = button; return; }
		if (button.dataset.check !== undefined && input.value.trim()) check(input.value.trim(), button.dataset.check === '1');
	});
	document.getElementById('wordForm').addEventListener('submit', function (e) {
		e.preventDefault();
		var allowed = !!(lastSubmitter && lastSubmitter.dataset.allowed === '1');
		api.action('/api/chat_filter/words', { word: input.value.trim(), allowed: allowed }).then(function (d) {
			toast(d.message, 'success');
			input.value = '';
		}).catch(function () {});
	});

	document.getElementById('chatFilterPage').addEventListener('click', function (e) {
		var pick = e.target.closest('[data-pick]'), remove = e.target.closest('[data-remove]');
		if (pick) {
			input.value = pick.dataset.pick;
			check(pick.dataset.pick, false);
		} else if (remove && confirm('Remove "' + remove.dataset.remove + '"? The filter\'s files decide about it again.')) {
			api.action('/api/chat_filter/words/delete', { word: remove.dataset.remove }).then(function (d) { toast(d.message, 'success'); }).catch(function () {});
		}
	});
	document.getElementById('reloadWorlds').addEventListener('click', function () { api.action('/api/chat_filter/reload', {}).catch(function () {}); });

	fileSearch.addEventListener('input', function () { clearTimeout(fileTimer); fileTimer = setTimeout(function () { loadFiles(false); }, 250); });
	document.getElementById('fileMore').addEventListener('click', function () { loadFiles(true); });
	document.getElementById('importFile').addEventListener('click', function () {
		if (!confirm('Copy every word of the allowed words file into the Allowed list, so each can be removed or blocked here? Words already on a list stay as they are.')) return;
		api.action('/api/chat_filter/import', {}).then(function (d) { toast(d.message, 'success'); }).catch(function () {});
	});

	if (window.Live) Live.on('chat_filter', function () { load(); loadFiles(false); });
	load();
	loadFiles(false);
})();
