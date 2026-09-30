/**
 * The Chat Filter page: test a message against the filter, the staff Blocked and Allowed lists (search, paging, add,
 * move and remove, each confirmed with what it changes in recent chat), and the filter's word files.
 */
(function () {
	'use strict';

	var page = document.getElementById('chatFilterPage');
	var canChat = !!page.dataset.canChat;
	var PAGE = 50;
	function isPhrase(word) { return String(word).indexOf(' ') !== -1; }
	function phraseBadge(word) { return isPhrase(word) ? ' ' + fmt.badge('Phrase', 'secondary') : ''; }

	var REASONS = {
		blocked_here: 'Blocked on the staff list',
		allowed_here: 'Allowed on the staff list',
		allow_file: 'In chatplus_en_us.txt',
		character_name: 'An approved character name',
		not_allowed: 'Not in chatplus_en_us.txt or the Allowed list',
		block_file: 'In blocklist.dcf',
		not_in_block_file: 'Not a blocked word',
		no_block_file: 'No blocklist.dcf: free chat stops every word'
	};

	// ---- test a message ----

	var testForm = document.getElementById('testForm');
	var testMessage = document.getElementById('testMessage');

	function wordAction(w) {
		if (!w.word) return '';
		if (w.reason === 'blocked_here' || w.reason === 'allowed_here') return '<button type="button" class="btn btn-sm btn-outline-secondary" data-remove="' + esc(w.phrase || w.word) + '">Remove…</button>';
		if (w.phrase) return '';
		if (w.reason === 'not_allowed') return '<button type="button" class="btn btn-sm btn-outline-success" data-add="allowed" data-word="' + esc(w.word) + '">Allow…</button>';
		return '<button type="button" class="btn btn-sm btn-outline-danger" data-add="blocked" data-word="' + esc(w.word) + '">Block…</button>';
	}

	function runTest() {
		var message = testMessage.value;
		if (!message.trim()) return;
		var chat = document.getElementById('testChat').value;
		api.get('/api/chat_filter/test?chat=' + chat + '&message=' + encodeURIComponent(message)).then(function (d) {
			if (!d.success) { toast(d.error || 'Test failed', 'danger'); return; }
			var stopped = d.words.filter(function (w) { return w.stopped; });
			var verdict = document.getElementById('testVerdict');
			verdict.className = 'alert mb-2 py-2 alert-' + (stopped.length ? 'danger' : 'success');
			verdict.textContent = stopped.length
				? 'Stopped: ' + stopped.length + ' of ' + d.words.length + ' word(s) not allowed. Nobody would see it.'
				: 'Sent: every word is allowed.';
			document.getElementById('testWords').innerHTML = d.words.map(function (w) {
				return '<span class="badge ' + (w.stopped ? 'text-bg-danger' : 'text-bg-success') + '" title="' + esc(REASONS[w.reason] || w.reason) + '">' + esc(w.text || '(empty)') + '</span>';
			}).join('');
			document.getElementById('testRows').innerHTML = d.words.map(function (w) {
				return '<tr><td>' + esc(w.word || '(empty: two spaces in a row)') + '</td><td>' + (w.stopped ? fmt.badge('Stopped', 'danger') : fmt.badge('OK', 'success')) + '</td>' +
					'<td class="small">' + esc(REASONS[w.reason] || w.reason) + (w.phrase ? ' (the phrase <strong>' + esc(w.phrase) + '</strong>)' : '') + '</td><td class="text-end">' + wordAction(w) + '</td></tr>';
			}).join('');
			document.getElementById('testResult').classList.remove('d-none');
		}).catch(function () {});
	}

	function testWord(word) {
		testMessage.value = word;
		runTest();
		testForm.scrollIntoView({ behavior: 'smooth', block: 'center' });
	}

	testForm.addEventListener('submit', function (e) { e.preventDefault(); runTest(); });
	document.getElementById('testChat').addEventListener('change', runTest);

	// ---- staff lists ----

	var words = [], listStart = 0;

	function listed() {
		var which = (document.querySelector('input[name="listFilter"]:checked') || {}).value || '';
		var search = document.getElementById('listSearch').value.trim().toLowerCase();
		return words.filter(function (w) {
			if (which === 'blocked' && w.allowed) return false;
			if (which === 'allowed' && !w.allowed) return false;
			return !search || w.word.indexOf(search) !== -1 || String(w.added_by).toLowerCase().indexOf(search) !== -1;
		});
	}

	function renderList() {
		var rows = listed();
		if (listStart >= rows.length) listStart = Math.max(0, Math.floor((rows.length - 1) / PAGE) * PAGE);
		var shown = rows.slice(listStart, listStart + PAGE);
		document.getElementById('listRows').innerHTML = shown.map(function (w) {
			var other = w.allowed ? 'blocked' : 'allowed';
			return '<tr><td>' + esc(w.word) + phraseBadge(w.word) + '</td><td>' + (w.allowed ? fmt.badge('Allowed', 'success') : fmt.badge('Blocked', 'danger')) + '</td>' +
				'<td class="small">' + esc(w.added_by) + '</td><td class="small text-nowrap">' + esc(fmt.unix(w.added_at)) + '</td>' +
				'<td class="text-end text-nowrap"><button type="button" class="btn btn-sm btn-outline-secondary" data-test="' + esc(w.word) + '">Test</button> ' +
				(isPhrase(w.word) && !w.allowed ? '' : '<button type="button" class="btn btn-sm btn-outline-' + (w.allowed ? 'danger' : 'success') + '" data-add="' + other + '" data-word="' + esc(w.word) + '">' + (w.allowed ? 'Block…' : 'Allow…') + '</button> ') +
				'<button type="button" class="btn btn-sm btn-outline-secondary" data-remove="' + esc(w.word) + '">Remove…</button></td></tr>';
		}).join('') || '<tr><td colspan="5" class="text-body-secondary">' + (words.length ? 'No words match.' : 'No words added yet.') + '</td></tr>';
		document.getElementById('listRange').textContent = rows.length ? (listStart + 1) + '–' + (listStart + shown.length) + ' of ' + rows.length : '';
		document.getElementById('listPrev').disabled = listStart === 0;
		document.getElementById('listNext').disabled = listStart + PAGE >= rows.length;
	}

	function loadList() {
		api.get('/api/chat_filter').then(function (d) {
			if (!d.success) return;
			words = d.words.sort(function (a, b) { return a.word < b.word ? -1 : a.word > b.word ? 1 : 0; });
			var blocked = words.filter(function (w) { return !w.allowed; }).length;
			document.getElementById('blockedCount').textContent = blocked + ' blocked';
			document.getElementById('allowedCount').textContent = (words.length - blocked) + ' allowed';
			renderList();
		}).catch(function () {});
	}

	document.querySelectorAll('input[name="listFilter"]').forEach(function (r) { r.addEventListener('change', function () { listStart = 0; renderList(); }); });
	document.getElementById('listSearch').addEventListener('input', function () { listStart = 0; renderList(); });
	document.getElementById('listPrev').addEventListener('click', function () { listStart = Math.max(0, listStart - PAGE); renderList(); });
	document.getElementById('listNext').addEventListener('click', function () { listStart += PAGE; renderList(); });

	var addForm = document.getElementById('addForm'), addWord = document.getElementById('addWord'), addList = 'blocked';
	addForm.addEventListener('click', function (e) { var b = e.target.closest('button[data-list]'); if (b) addList = b.dataset.list; });
	addForm.addEventListener('submit', function (e) {
		e.preventDefault();
		var word = addWord.value.trim().replace(/\s+/g, ' ');
		if (!word) return;
		if (addList === 'allowed' && isPhrase(word)) { toast('Phrases can only be blocked: normal chat checks each word on its own, so allow the words instead', 'warning'); return; }
		confirmAdd(word, addList === 'allowed').then(function (done) { if (done) addWord.value = ''; });
	});

	// ---- confirmation: what the change does, from the word's standing and recent chat ----

	var modalEl = document.getElementById('cfConfirm');
	var modal = new bootstrap.Modal(modalEl);
	var onGo = null;

	function standing(d) {
		var parts = [d.inAllowFile ? 'in chatplus_en_us.txt' : 'not in chatplus_en_us.txt'];
		if (d.inBlockFile) parts.push('in blocklist.dcf');
		parts.push(d.dashboard ? (d.dashboard === 'blocked' ? 'on the Blocked list' : 'on the Allowed list') : 'on neither staff list');
		return '<strong>' + esc(d.word) + '</strong>' + phraseBadge(d.word) + ' now: ' + esc(parts.join(', ')) + '.';
	}

	function openConfirm(options) {
		document.getElementById('cfConfirmTitle').textContent = options.title;
		document.getElementById('cfConfirmText').textContent = options.text;
		document.getElementById('cfConfirmStanding').innerHTML = '';
		document.getElementById('cfConfirmImpact').classList.add('d-none');
		var go = document.getElementById('cfConfirmGo');
		go.className = 'btn btn-' + options.tone;
		go.textContent = options.button;
		go.disabled = false;
		return new Promise(function (resolve) {
			var settled = false;
			onGo = function () {
				go.disabled = true;
				options.run().then(function () { settled = true; resolve(true); modal.hide(); }).catch(function () { go.disabled = false; });
			};
			modalEl.addEventListener('hidden.bs.modal', function once() { modalEl.removeEventListener('hidden.bs.modal', once); if (!settled) resolve(false); });
			modal.show();
		});
	}

	document.getElementById('cfConfirmGo').addEventListener('click', function () { if (onGo) onGo(); });

	function showStanding(word) {
		api.get('/api/chat_filter/lookup?word=' + encodeURIComponent(word)).then(function (d) {
			var el = document.getElementById('cfConfirmStanding');
			if (!d.success) { el.textContent = d.error || ''; return; }
			el.innerHTML = standing(d);
		}).catch(function () {});
	}

	// allowed=false: recent messages players saw that blocking would stop; true: recent stopped messages with the word
	function showImpact(word, allowed) {
		var box = document.getElementById('cfConfirmImpact');
		var title = document.getElementById('cfImpactTitle');
		var rows = document.getElementById('cfImpactRows');
		box.classList.remove('d-none');
		if (!canChat) { title.textContent = 'Recent chat'; rows.innerHTML = '<tr><td colspan="4" class="text-body-secondary">Seeing recent chat needs the chat_view permission.</td></tr>'; return; }
		title.textContent = 'Checking recent chat…';
		rows.innerHTML = '';
		api.get('/api/chat_filter/check?word=' + encodeURIComponent(word) + '&allowed=' + (allowed ? 1 : 0)).then(function (d) {
			if (!d.success) { title.textContent = d.error || 'Check failed'; return; }
			title.textContent = allowed
				? d.messages.length + ' recent stopped message(s) contain it (they still need their other words allowed)'
				: d.messages.length + ' recent message(s) players saw would have been stopped';
			rows.innerHTML = d.messages.map(function (m) {
				return '<tr><td class="small text-nowrap">' + esc(fmt.unix(m.time)) + '</td><td class="small">' + (m.zone_id ? fmt.zone(m.zone_id, m.zone_name) : esc(m.channel)) + '</td>' +
					'<td>' + fmt.character(m.sender_id, m.sender_name) + '</td><td>' + esc(m.message) + '</td></tr>';
			}).join('') || '<tr><td colspan="4" class="text-body-secondary">None in the newest ' + esc(d.limit) + ' messages with this text.</td></tr>';
		}).catch(function () {});
	}

	function confirmAdd(word, allowed) {
		var p = openConfirm({
			title: (allowed ? 'Allow "' : 'Block "') + word + '"?',
			text: allowed ? 'Players may use it in normal chat. Running worlds apply it at once.'
				: (isPhrase(word) ? 'The phrase is stopped in all chat when its words come in a row. Running worlds apply it at once.' : 'It is stopped in all chat, even where a word file allows it. Running worlds apply it at once.'),
			tone: allowed ? 'success' : 'danger',
			button: allowed ? 'Allow' : 'Block',
			run: function () { return api.action('/api/chat_filter/words', { word: word, allowed: allowed }).then(function (d) { toast(d.message, 'success'); }); }
		});
		showStanding(word);
		showImpact(word, allowed);
		return p;
	}

	function confirmRemove(word) {
		var p = openConfirm({
			title: 'Remove "' + word + '"?',
			text: 'It comes off the staff lists and the word files decide about it again.',
			tone: 'secondary',
			button: 'Remove',
			run: function () { return api.action('/api/chat_filter/words/delete', { word: word }).then(function (d) { toast(d.message, 'success'); }); }
		});
		showStanding(word);
		return p;
	}

	page.addEventListener('click', function (e) {
		var add = e.target.closest('[data-add]'), remove = e.target.closest('[data-remove]'), test = e.target.closest('[data-test]');
		if (add) confirmAdd(add.dataset.word, add.dataset.add === 'allowed');
		else if (remove) confirmRemove(remove.dataset.remove);
		else if (test) testWord(test.dataset.test);
	});

	// ---- word files ----

	var fileSearch = document.getElementById('fileSearch'), fileStart = 0, fileTimer = null, filePage = 200;

	function loadFiles() {
		api.get('/api/chat_filter/files?search=' + encodeURIComponent(fileSearch.value.trim()) + '&start=' + fileStart).then(function (d) {
			if (!d.success) return;
			filePage = d.pageSize || filePage;
			document.getElementById('fileSummary').innerHTML =
				'<li><code>' + esc(d.allowFile) + '</code>: ' + (d.allowFileFound
					? esc(d.allowTotal) + ' words normal chat may use (from the client); ' + esc(d.onDashboard) + ' are on a staff list too, which wins.'
					: 'could not be read. Set <code>client_location</code>.') + '</li>' +
				'<li><code>' + esc(d.blockFile) + '</code>: ' + (d.blockFileFound
					? esc(d.blockTotal) + ' words' + (d.blockMaxWords > 1 ? ' and phrases (up to ' + esc(d.blockMaxWords) + ' words)' : '') + ' best friends\' free chat may not use. Stored as hashes, so they can\'t be listed; test a word or phrase to see if it is one.'
					: d.blockFileOld
						? 'in the old format (its hashes depended on the platform), so the servers can\'t read it and best friends\' free chat stops everything.'
						: 'not found next to the servers (' + esc(d.blockFileStatus) + '), so best friends\' free chat stops everything.') +
					' To change it, put the words in <code>' + esc(d.blockText) + '</code> next to the servers (one word or phrase per line) and start the servers again; they build <code>' + esc(d.blockFile) + '</code> from it.</li>';
			document.getElementById('fileWords').innerHTML = d.words.map(function (w) {
				var cls = w.dashboard === 'blocked' ? 'text-bg-danger' : w.dashboard === 'allowed' ? 'text-bg-success' : 'text-bg-secondary';
				return '<button type="button" class="badge border-0 ' + cls + '" data-test="' + esc(w.word) + '">' + esc(w.word) + '</button>';
			}).join('') || '<span class="text-body-secondary small">No words match.</span>';
			document.getElementById('fileRange').textContent = d.matched ? (fileStart + 1) + '–' + (fileStart + d.words.length) + ' of ' + d.matched : '';
			document.getElementById('filePrev').disabled = fileStart === 0;
			document.getElementById('fileNext').disabled = fileStart + d.words.length >= d.matched;
		}).catch(function () {});
	}

	fileSearch.addEventListener('input', function () { clearTimeout(fileTimer); fileTimer = setTimeout(function () { fileStart = 0; loadFiles(); }, 250); });
	document.getElementById('filePrev').addEventListener('click', function () { fileStart = Math.max(0, fileStart - filePage); loadFiles(); });
	document.getElementById('fileNext').addEventListener('click', function () { fileStart += filePage; loadFiles(); });
	document.getElementById('importFile').addEventListener('click', function () {
		openConfirm({
			title: 'Copy the file\'s words into Allowed?',
			text: 'Every word of chatplus_en_us.txt goes on the Allowed list once, so each can be removed or blocked here. Words already on a list stay as they are; the file is not changed.',
			tone: 'primary',
			button: 'Copy',
			run: function () { return api.action('/api/chat_filter/import', {}).then(function (d) { toast(d.message, 'success'); }); }
		});
	});

	document.getElementById('reloadWorlds').addEventListener('click', function () { api.action('/api/chat_filter/reload', {}).catch(function () {}); });

	if (window.Live) Live.on('chat_filter', function () {
		loadList();
		loadFiles();
		if (!document.getElementById('testResult').classList.contains('d-none')) runTest();
	});
	loadList();
	loadFiles();
})();
