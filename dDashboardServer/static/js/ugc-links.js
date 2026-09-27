/**
 * Players' creations on the property and character pages: the icon the UGC server made of a model, car or rocket in
 * place of the item's own icon, its state and a link to it on the /ugc page (for those who may open it). The routes
 * (/api/ugc_links/...) let whoever may view the page see the icons of the creations on it.
 */
(function () {
	var STATES = { done: ['Made', 'success'], pending: ['Waiting', 'secondary'], failed: ['Failed', 'danger'] };

	window.UgcLinks = {
		// A badge for the UGC server's state; a failure's reason as its tooltip
		state: function (item) {
			var s = STATES[item.state] || [item.state, 'secondary'];
			return '<span class="badge text-bg-' + s[1] + '"' + (item.error ? ' title="' + esc(item.error) + '"' : '') + '>' + esc(s[0]) + '</span>';
		},
		// An <img> of the creation's icon that falls back to the item's own icon until the UGC server made one
		icon: function (item, lot, size) {
			size = size || 40;
			return '<img src="' + esc(item.icon) + '" width="' + size + '" height="' + size + '" class="rounded bg-body-tertiary" alt="" loading="lazy" ' +
				'onerror="this.onerror=null;this.src=\'/api/icon/' + esc(lot) + '\'">';
		},
		// {modelId: item} for the player-built models on a property
		forProperty: function (propertyId) {
			return api.get('/api/ugc_links/property/' + propertyId).then(function (d) {
				var map = {};
				((d && d.items) || []).forEach(function (item) { map[item.modelId] = item; });
				return map;
			}).catch(function () { return {}; });
		},
		// Shows the creations among a character page's items (.inv-item[data-id]) with their own icon and a link
		decorateInventory: function (characterId, root) {
			return api.get('/api/ugc_links/character/' + characterId).then(function (d) {
				((d && d.items) || []).forEach(function (item) {
					(root || document).querySelectorAll('.inv-item[data-id="' + item.itemId + '"]').forEach(function (el) {
						var img = el.querySelector('img');
						if (img) {
							var own = img.src;
							img.onerror = function () { img.onerror = null; img.src = own; };
							img.src = item.icon;
						}
						el.classList.add('inv-ugc');
						el.title = (el.title ? el.title + ' - ' : '') + 'Player-built ' + (item.kind === 'modular' ? 'car or rocket' : 'model') + ' (UGC ' + item.ugcId + ', ' + item.state + ')';
						if (item.link && !el.querySelector('.inv-ugc-link')) {
							// Items may sit inside a link already (the item trace), so this is a button, not a nested link
							var a = document.createElement('span');
							a.className = 'inv-ugc-link';
							a.setAttribute('role', 'link');
							a.tabIndex = 0;
							a.textContent = 'UGC';
							a.title = 'Open in the UGC viewer';
							var open = function (e) { e.preventDefault(); e.stopPropagation(); window.location.href = item.link; };
							a.addEventListener('click', open);
							a.addEventListener('keydown', function (e) { if (e.key === 'Enter') open(e); });
							el.appendChild(a);
						}
					});
				});
			}).catch(function () {});
		}
	};
})();
