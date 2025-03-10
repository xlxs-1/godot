/*************************************************************************/
/*  tile_map.h                                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef TILE_MAP_H
#define TILE_MAP_H

#include "core/templates/self_list.h"
#include "core/templates/vset.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/resources/tile_set.h"

class TileSetAtlasSource;

union TileMapCell {
	struct {
		int32_t source_id : 16;
		int16_t coord_x : 16;
		int16_t coord_y : 16;
		int32_t alternative_tile : 16;
	};

	uint64_t _u64t;
	TileMapCell(int p_source_id = -1, Vector2i p_atlas_coords = TileSetSource::INVALID_ATLAS_COORDS, int p_alternative_tile = TileSetSource::INVALID_TILE_ALTERNATIVE) {
		source_id = p_source_id;
		set_atlas_coords(p_atlas_coords);
		alternative_tile = p_alternative_tile;
	}

	Vector2i get_atlas_coords() const {
		return Vector2i(coord_x, coord_y);
	}

	void set_atlas_coords(const Vector2i &r_coords) {
		coord_x = r_coords.x;
		coord_y = r_coords.y;
	}

	bool operator<(const TileMapCell &p_other) const {
		if (source_id == p_other.source_id) {
			if (coord_x == p_other.coord_x) {
				if (coord_y == p_other.coord_y) {
					return alternative_tile < p_other.alternative_tile;
				} else {
					return coord_y < p_other.coord_y;
				}
			} else {
				return coord_x < p_other.coord_x;
			}
		} else {
			return source_id < p_other.source_id;
		}
	}

	bool operator!=(const TileMapCell &p_other) const {
		return !(source_id == p_other.source_id && coord_x == p_other.coord_x && coord_y == p_other.coord_y && alternative_tile == p_other.alternative_tile);
	}
};

struct TileMapQuadrant {
	struct CoordsWorldComparator {
		_ALWAYS_INLINE_ bool operator()(const Vector2i &p_a, const Vector2i &p_b) const {
			// We sort the cells by their world coords, as it is needed by rendering.
			if (p_a.y == p_b.y) {
				return p_a.x > p_b.x;
			} else {
				return p_a.y < p_b.y;
			}
		}
	};

	// Dirty list element
	SelfList<TileMapQuadrant> dirty_list_element;

	// Quadrant layer and coords.
	int layer = -1;
	Vector2i coords;

	// TileMapCells
	Set<Vector2i> cells;
	// We need those two maps to sort by world position for rendering
	// This is kind of workaround, it would be better to sort the cells directly in the "cells" set instead.
	Map<Vector2i, Vector2i> map_to_world;
	Map<Vector2i, Vector2i, CoordsWorldComparator> world_to_map;

	// Debug.
	RID debug_canvas_item;

	// Rendering.
	List<RID> canvas_items;
	List<RID> occluders;

	// Physics.
	List<RID> bodies;

	// Navigation.
	Map<Vector2i, Vector<RID>> navigation_regions;

	// Scenes.
	Map<Vector2i, String> scenes;

	void operator=(const TileMapQuadrant &q) {
		layer = q.layer;
		coords = q.coords;
		debug_canvas_item = q.debug_canvas_item;
		canvas_items = q.canvas_items;
		occluders = q.occluders;
		bodies = q.bodies;
		navigation_regions = q.navigation_regions;
	}

	TileMapQuadrant(const TileMapQuadrant &q) :
			dirty_list_element(this) {
		layer = q.layer;
		coords = q.coords;
		debug_canvas_item = q.debug_canvas_item;
		canvas_items = q.canvas_items;
		occluders = q.occluders;
		bodies = q.bodies;
		navigation_regions = q.navigation_regions;
	}

	TileMapQuadrant() :
			dirty_list_element(this) {
	}
};

class TileMapPattern : public Object {
	GDCLASS(TileMapPattern, Object);

	Vector2i size;
	Map<Vector2i, TileMapCell> pattern;

protected:
	static void _bind_methods();

public:
	void set_cell(const Vector2i &p_coords, int p_source_id, const Vector2i p_atlas_coords, int p_alternative_tile = 0);
	bool has_cell(const Vector2i &p_coords) const;
	void remove_cell(const Vector2i &p_coords, bool p_update_size = true);
	int get_cell_source_id(const Vector2i &p_coords) const;
	Vector2i get_cell_atlas_coords(const Vector2i &p_coords) const;
	int get_cell_alternative_tile(const Vector2i &p_coords) const;

	TypedArray<Vector2i> get_used_cells() const;

	Vector2i get_size() const;
	void set_size(const Vector2i &p_size);
	bool is_empty() const;

	void clear();
};

class TileMap : public Node2D {
	GDCLASS(TileMap, Node2D);

public:
	enum VisibilityMode {
		VISIBILITY_MODE_DEFAULT,
		VISIBILITY_MODE_FORCE_SHOW,
		VISIBILITY_MODE_FORCE_HIDE,
	};

private:
	friend class TileSetPlugin;

	// A compatibility enum to specify how is the data if formatted.
	enum DataFormat {
		FORMAT_1 = 0,
		FORMAT_2,
		FORMAT_3
	};
	mutable DataFormat format = FORMAT_1; // Assume lowest possible format if none is present;

	static constexpr float FP_ADJUST = 0.00001;

	// Properties.
	Ref<TileSet> tile_set;
	int quadrant_size = 16;
	VisibilityMode collision_visibility_mode = VISIBILITY_MODE_DEFAULT;
	VisibilityMode navigation_visibility_mode = VISIBILITY_MODE_DEFAULT;

	// Updates.
	bool pending_update = false;

	// Rect.
	Rect2 rect_cache;
	bool rect_cache_dirty = true;
	Rect2i used_rect_cache;
	bool used_rect_cache_dirty = true;

	// TileMap layers.
	struct TileMapLayer {
		String name;
		bool enabled = true;
		bool y_sort_enabled = false;
		int y_sort_origin = 0;
		int z_index = 0;
		RID canvas_item;
		Map<Vector2i, TileMapCell> tile_map;
		Map<Vector2i, TileMapQuadrant> quadrant_map;
		SelfList<TileMapQuadrant>::List dirty_quadrant_list;
	};
	LocalVector<TileMapLayer> layers;
	int selected_layer = -1;

	// Quadrants and internals management.
	Vector2i _coords_to_quadrant_coords(int p_layer, const Vector2i &p_coords) const;

	Map<Vector2i, TileMapQuadrant>::Element *_create_quadrant(int p_layer, const Vector2i &p_qk);

	void _make_quadrant_dirty(Map<Vector2i, TileMapQuadrant>::Element *Q);
	void _make_all_quadrants_dirty();
	void _queue_update_dirty_quadrants();

	void _update_dirty_quadrants();

	void _recreate_internals();

	void _erase_quadrant(Map<Vector2i, TileMapQuadrant>::Element *Q);
	void _clear_layer_internals(int p_layer);
	void _clear_internals();

	// Rect caching.
	void _recompute_rect_cache();

	// Per-system methods.
	bool _rendering_quadrant_order_dirty = false;
	void _rendering_notification(int p_what);
	void _rendering_update_layer(int p_layer);
	void _rendering_cleanup_layer(int p_layer);
	void _rendering_update_dirty_quadrants(SelfList<TileMapQuadrant>::List &r_dirty_quadrant_list);
	void _rendering_create_quadrant(TileMapQuadrant *p_quadrant);
	void _rendering_cleanup_quadrant(TileMapQuadrant *p_quadrant);
	void _rendering_draw_quadrant_debug(TileMapQuadrant *p_quadrant);

	void _physics_notification(int p_what);
	void _physics_update_dirty_quadrants(SelfList<TileMapQuadrant>::List &r_dirty_quadrant_list);
	void _physics_create_quadrant(TileMapQuadrant *p_quadrant);
	void _physics_cleanup_quadrant(TileMapQuadrant *p_quadrant);
	void _physics_draw_quadrant_debug(TileMapQuadrant *p_quadrant);

	void _navigation_notification(int p_what);
	void _navigation_update_dirty_quadrants(SelfList<TileMapQuadrant>::List &r_dirty_quadrant_list);
	void _navigation_cleanup_quadrant(TileMapQuadrant *p_quadrant);
	void _navigation_draw_quadrant_debug(TileMapQuadrant *p_quadrant);

	void _scenes_update_dirty_quadrants(SelfList<TileMapQuadrant>::List &r_dirty_quadrant_list);
	void _scenes_cleanup_quadrant(TileMapQuadrant *p_quadrant);
	void _scenes_draw_quadrant_debug(TileMapQuadrant *p_quadrant);

	// Set and get tiles from data arrays.
	void _set_tile_data(int p_layer, const Vector<int> &p_data);
	Vector<int> _get_tile_data(int p_layer) const;

	void _tile_set_changed();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _notification(int p_what);
	static void _bind_methods();

public:
	static Vector2i transform_coords_layout(Vector2i p_coords, TileSet::TileOffsetAxis p_offset_axis, TileSet::TileLayout p_from_layout, TileSet::TileLayout p_to_layout);

	enum {
		INVALID_CELL = -1
	};

#ifdef TOOLS_ENABLED
	virtual Rect2 _edit_get_rect() const override;
#endif

	void set_tileset(const Ref<TileSet> &p_tileset);
	Ref<TileSet> get_tileset() const;

	void set_quadrant_size(int p_size);
	int get_quadrant_size() const;

	static void draw_tile(RID p_canvas_item, Vector2i p_position, const Ref<TileSet> p_tile_set, int p_atlas_source_id, Vector2i p_atlas_coords, int p_alternative_tile, Color p_modulation = Color(1.0, 1.0, 1.0, 1.0));

	// Layers management.
	void set_layers_count(int p_layers_count);
	int get_layers_count() const;
	void set_layer_name(int p_layer, String p_name);
	String get_layer_name(int p_layer) const;
	void set_layer_enabled(int p_layer, bool p_visible);
	bool is_layer_enabled(int p_layer) const;
	void set_layer_y_sort_enabled(int p_layer, bool p_enabled);
	bool is_layer_y_sort_enabled(int p_layer) const;
	void set_layer_y_sort_origin(int p_layer, int p_y_sort_origin);
	int get_layer_y_sort_origin(int p_layer) const;
	void set_layer_z_index(int p_layer, int p_z_index);
	int get_layer_z_index(int p_layer) const;
	void set_selected_layer(int p_layer_id); // For editor use.
	int get_selected_layer() const;

	void set_collision_visibility_mode(VisibilityMode p_show_collision);
	VisibilityMode get_collision_visibility_mode();

	void set_navigation_visibility_mode(VisibilityMode p_show_navigation);
	VisibilityMode get_navigation_visibility_mode();

	void set_cell(int p_layer, const Vector2i &p_coords, int p_source_id = -1, const Vector2i p_atlas_coords = TileSetSource::INVALID_ATLAS_COORDS, int p_alternative_tile = TileSetSource::INVALID_TILE_ALTERNATIVE);
	int get_cell_source_id(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	Vector2i get_cell_atlas_coords(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	int get_cell_alternative_tile(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;

	TileMapPattern *get_pattern(int p_layer, TypedArray<Vector2i> p_coords_array);
	Vector2i map_pattern(Vector2i p_position_in_tilemap, Vector2i p_coords_in_pattern, const TileMapPattern *p_pattern);
	void set_pattern(int p_layer, Vector2i p_position, const TileMapPattern *p_pattern);

	// Not exposed to users
	TileMapCell get_cell(int p_layer, const Vector2i &p_coords, bool p_use_proxies = false) const;
	Map<Vector2i, TileMapQuadrant> *get_quadrant_map(int p_layer);
	int get_effective_quadrant_size(int p_layer) const;
	//---

	virtual void set_y_sort_enabled(bool p_enable) override;

	Vector2 map_to_world(const Vector2i &p_pos) const;
	Vector2i world_to_map(const Vector2 &p_pos) const;

	bool is_existing_neighbor(TileSet::CellNeighbor p_cell_neighbor) const;
	Vector2i get_neighbor_cell(const Vector2i &p_coords, TileSet::CellNeighbor p_cell_neighbor) const;

	TypedArray<Vector2i> get_used_cells(int p_layer) const;
	Rect2 get_used_rect(); // Not const because of cache

	// Override some methods of the CanvasItem class to pass the changes to the quadrants CanvasItems
	virtual void set_light_mask(int p_light_mask) override;
	virtual void set_material(const Ref<Material> &p_material) override;
	virtual void set_use_parent_material(bool p_use_parent_material) override;
	virtual void set_texture_filter(CanvasItem::TextureFilter p_texture_filter) override;
	virtual void set_texture_repeat(CanvasItem::TextureRepeat p_texture_repeat) override;

	// Fixing a nclearing methods.
	void fix_invalid_tiles();

	void clear_layer(int p_layer);
	void clear();

	// Helpers
	TypedArray<Vector2i> get_surrounding_tiles(Vector2i coords);
	void draw_cells_outline(Control *p_control, Set<Vector2i> p_cells, Color p_color, Transform2D p_transform = Transform2D());

	// Configuration warnings.
	TypedArray<String> get_configuration_warnings() const override;

	TileMap();
	~TileMap();
};

VARIANT_ENUM_CAST(TileMap::VisibilityMode);

#endif // TILE_MAP_H
