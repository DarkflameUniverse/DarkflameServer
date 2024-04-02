# DLU Vanity System

Darkflame Universe Vanity System is a method of defined objects to be spawned serverside and communicated to the client without modifying the game's assets.

You can check out the different `xml` files in `vanity/` and use `vanity/demo.xml` to follow along with this tutorial and documentation of thew various features, cabapilities, and limitations of this system.

## `vanity/root.xml`
`root.xml` is the only file the server will load in by default.

To load other files, you can do so like this:
```xml
<files>
	<file name="dev-tribute.xml" enabled="1"/>
	<file name="atm.xml" enabled="0"/>
	<file name="demo.xml" enabled="0"/>
</files>
```

`name` is the name of the file relative to the vanity folder.
Ex: you have a folder like `vanity/events/` with a file called `halloween.xml` in it, you will include it as such:

`<file name="events/halloween.xml" enabled="1"/>`

`enabled` tells if that file should be loaded.
files will only be loaded in, if `enabled="1"`

There cannot be multiple `<files></files>` per xml file, only the first one will be read, but you can have as many `<files/>` in it as you wish.

## `vanity/demo.xml`

This demo file covers most of the features of defining objects to spawn server side and will go over them one by one.

The minimun data needed to define and ojbect to spawn is as follows:
```xml
<objects>
	<object lot="1">
		<locations>
			<location zone="1200" x="0" y="0" z="0" rw="0" rx="0" ry="0" rz="0" />
		</locations>
	</object>
</objects>
```

 * `lot` the LEGO Object Template to be spawned

 * `<location/>` must have `zone`, `x`, `y`, `z` `rw`, `rx`, `ry`, and `rz` and one `location` must in exist in `locations` for it to be spawned

Everything else is is optional.

* LEGO Name Value (LNV) configs can control almost all functionality of the objects in the game, they are defined like `name=type:value`. types can be found in `dCommon/LDFFromat.h`

```xml
<config>
	<key>bool=7:1</key>
</config>
```

### Story Plaque with custom text

lot 8193 is the story plaque that is used in game to give the game lore to the player.

DLU Vanity has the capability to provide custom text to it via a LNV config

From `demo.xml`
```xml
	<object lot="8139">
		<config>
			<key>customText=13:This story plaque has custom text that is defined by DLU's vanity system. Check out &lt;font color="#000000" &gt;vanity/demo.xml&lt;/font&gt; to see how this works!</key>
		</config>
		<locations>
			<location zone="1200" x="-26.281" y="288.896" z="-77.484" rw="0.997534" rx="0.00" ry="-0.070190" rz="0.00" />
		</locations>
	</object>
```

 * The `customText` config must be a type of `13` (wstring)
 * HTML like formatting can be used for font color, but `<` must be reaplaced withh `&gt;`, and `>` replaced with `&lt;`

### Object with multiple locations and scale

```xml
	<object lot="3248">
		<locations>
			<location zone="1200" x="-15.0" y="288.8" z="-167.0" rw="0.984321" rx="0.00" ry="0.176388" rz="0.00" />
			<location zone="1200" x="15.0" y="288.8" z="-158.0" rw="0.724628" rx="0.00" ry="-0.689141" rz="0.00" scale="0.30" />
		</locations>
	</object>
```
#### Multiple locations
Multiple location elements can be defined for an object.
For every location that object will spawn:
This will spawn two trees, one at each specified location.

#### Scale
Each location can specify a `scale` which defaults to `1`. The object will be scaled by this attribute in the location when defined.

### Object with multiple random locations and chance

```xml
	<object lot="10141">
		<config>
			<key>useLocationsAsRandomSpawnPoint=7:1</key>
		</config>
		<locations>
			<location zone="1200" x="31.819" y="288.896" z="-117.095" rw="0.630659" rx="0.00" ry="-0.776060" rz="0.00" chance="0.50"/>
			<location zone="1200" x="42.755" y="291.897" z="-144.385" rw="0.855306" rx="0.00" ry="-0.518124" rz="0.00" chance="0.50"/>
			<location zone="1200" x="3.984" y="288.896" z="-165.947" rw="0.978508" rx="0.00" ry="-0.206210" rz="0.00" chance="0.50"/>
		</locations>
	</object>
```
#### Random Spawn Point
If the LNV config `useLocationsAsRandomSpawnPoint=7:1` is defined and is set to `1`, instead of spawning the object at every location, it will randomly choose between all locations in the current zone to spawn one instance of the object.
If a location is not in the current zone, it will not be considered.

#### Chance
Each location can specify a `chance` attribute. This defines a chance from, `0` to `1` that the object will spawn, with `0` being never spawn, and `1` being always spawn. A `chance="0.8"` will have an 80% chance for the object to spawn at that location.

`useLocationsAsRandomSpawnPoint` and `chance` are independent of each other and be used separately

### Custom vendor with custom name and gear

```xml
	<object name="Demo Fella - GM Items Vendor" lot="1867">
		<equipment>7630, 1727, 7453, 7521</equipment>
		<config>
			<key>vendorInvOverride=0:1727,7292,16553,2243,14535,14538,14531,6730</key>
		</config>
		<locations>
			<location zone="1200" x="35.935" y="288.896" z="-128.213" rw="0.882977" rx="0.00" ry="-0.469416" rz="0.00" />
		</locations>
	</object>
```
#### Custom vendor

For a custom vendor to work, you must use a LOT that already has a vendor compoenet attached to it.

Without an LNV keys, it will give it's normal inventory
You muse define `vendorInvOverride=0:` and then a list of LOTs that the vendor will sell. This will override all items the vendor was selling and use the list of LOTs provided. 

LOTs must have an item component in order to be sold by a vendor.

#### Custom Name

The `name` attribute will give or override the name displayed for an object, if it displays one.
Using a ` - ` will allow you to define a title that will display under their name on their nametag (formally called a billboard).

#### Custom Gear

The `equipment` element is a lis of comma separated lots that will defind the geat an object will attempt to equip.

LOTs must have an inventory component in order to equipe custom gear.

### Creating Spawners
```xml
	<object lot="176">
		<config>
			...
			<key>spawntemplate=1:2295</key>
			...
		</config>
		<locations>
			<location zone="1200" x="4.232" y="288.895" z="-85.846" rw="-0.205988" rx="0.00" ry="0.978555" rz="0.00" />
		</locations>
	</object>
</objects>
```

LOT `167` is a spawner. The spawner config in the `demo.xml` was copied from an existing object via lcdr's luzviewer.

The main config to care about is `spawntemplate` as that controls what the spawner spawns.
