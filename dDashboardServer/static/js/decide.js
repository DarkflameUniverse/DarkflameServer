/**
 * The dialog for rejecting or removing something a player made (a name, pet name, property or leaderboard score): a reason,
 * and for staff who may give strikes, a choice to also put a strike on the player's account. Not every rejection deserves
 * one, so it starts unticked and shows how many strikes the account already has.
 *
 *   Decide({ title, action, reasonLabel, reasonRequired, characterId }).then(function (choice) { choice.reason; choice.strike; })
 *
 * Resolves with null when cancelled.
 */
(function () {
	'use strict';

	var modal = null, el = null, pending = null;

	function build() {
		el = document.createElement('div');
		el.className = 'modal fade';
		el.dataset.navKeep = ''; // shared by every page: kept when nav.js swaps pages
		el.tabIndex = -1;
		el.setAttribute('aria-labelledby', 'decideTitle');
		el.innerHTML =
			'<div class="modal-dialog"><form class="modal-content" id="decideForm">' +
			'<div class="modal-header"><h5 class="modal-title" id="decideTitle"></h5><button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button></div>' +
			'<div class="modal-body">' +
			'<p class="small text-body-secondary d-none" id="decideText"></p>' +
			'<label class="form-label" for="decideReason" id="decideReasonLabel"></label>' +
			'<textarea class="form-control" id="decideReason" rows="2" maxlength="300"></textarea>' +
			'<div class="form-check mt-3 d-none" id="decideStrikeWrap">' +
			'<input class="form-check-input" type="checkbox" id="decideStrike"><label class="form-check-label" for="decideStrike">Also give a strike on their account</label>' +
			'<div class="form-text" id="decideStrikeInfo"></div></div>' +
			'</div>' +
			'<div class="modal-footer"><button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cancel</button>' +
			'<button type="submit" class="btn btn-danger" id="decideSubmit"></button></div>' +
			'</form></div>';
		document.body.appendChild(el);
		modal = new bootstrap.Modal(el);
		el.querySelector('#decideForm').addEventListener('submit', function (e) {
			e.preventDefault();
			var reason = el.querySelector('#decideReason');
			if (reason.required && !reason.value.trim()) { reason.focus(); return; }
			var done = pending;
			pending = null;
			modal.hide();
			if (done) done({ reason: reason.value.trim(), strike: el.querySelector('#decideStrike').checked });
		});
		el.addEventListener('hidden.bs.modal', function () {
			if (pending) { var done = pending; pending = null; done(null); }
		});
		el.addEventListener('shown.bs.modal', function () { el.querySelector('#decideReason').focus(); });
	}

	window.Decide = function (options) {
		if (!el) build();
		options = options || {};
		// A prefill queued by another script (the AI helper's "Use this") for the next dialog: only filled in, never submitted
		var prefill = window.Decide.next || {};
		window.Decide.next = null;
		el.querySelector('#decideTitle').textContent = options.title || 'Reject';
		var text = el.querySelector('#decideText');
		text.textContent = options.text || '';
		text.classList.toggle('d-none', !options.text);
		el.querySelector('#decideReasonLabel').textContent = options.reasonLabel || 'Reason';
		var reason = el.querySelector('#decideReason');
		reason.value = options.reason || prefill.reason || '';
		reason.required = !!options.reasonRequired;
		el.querySelector('#decideSubmit').textContent = options.action || 'Reject';

		var strikeWrap = el.querySelector('#decideStrikeWrap'), strike = el.querySelector('#decideStrike'), info = el.querySelector('#decideStrikeInfo');
		var canStrike = DASH.can('strikes_give') && options.characterId && options.characterId !== '0';
		strike.checked = !!(canStrike && (options.strike || prefill.strike));
		strikeWrap.classList.toggle('d-none', !canStrike);
		info.textContent = '';
		if (canStrike) {
			api.get('/api/characters/' + options.characterId + '/strikes').then(function (d) {
				if (!d.success) return;
				info.innerHTML = 'Only if this deserves one. ' + esc(d.accountName || 'The account') + ' has ' +
					(d.active ? '<strong>' + esc(d.active) + '</strong> active strike' + (d.active === 1 ? '' : 's') : 'no active strikes') +
					(d.total > d.active ? ' (' + esc(d.total) + ' ever)' : '') + '.';
			}).catch(function () {});
		}

		return new Promise(function (resolve) {
			pending = resolve;
			modal.show();
		});
	};
})();
