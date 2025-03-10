/*************************************************************************/
/*  editor_inspector.cpp                                                 */
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

#include "editor_inspector.h"

#include "array_property_edit.h"
#include "dictionary_property_edit.h"
#include "editor/doc_tools.h"
#include "editor_feature_profile.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "multi_node_edit.h"
#include "scene/resources/packed_scene.h"

Size2 EditorProperty::get_minimum_size() const {
	Size2 ms;
	Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Tree"));
	int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Tree"));
	ms.height = font->get_height(font_size);

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}
		if (c->is_set_as_top_level()) {
			continue;
		}
		if (!c->is_visible()) {
			continue;
		}
		if (c == bottom_editor) {
			continue;
		}

		Size2 minsize = c->get_combined_minimum_size();
		ms.width = MAX(ms.width, minsize.width);
		ms.height = MAX(ms.height, minsize.height);
	}

	if (keying) {
		Ref<Texture2D> key = get_theme_icon(SNAME("Key"), SNAME("EditorIcons"));
		ms.width += key->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));
	}

	if (deletable) {
		Ref<Texture2D> key = get_theme_icon(SNAME("Close"), SNAME("EditorIcons"));
		ms.width += key->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));
	}

	if (checkable) {
		Ref<Texture2D> check = get_theme_icon(SNAME("checked"), SNAME("CheckBox"));
		ms.width += check->get_width() + get_theme_constant(SNAME("hseparation"), SNAME("CheckBox")) + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));
	}

	if (bottom_editor != nullptr && bottom_editor->is_visible()) {
		ms.height += get_theme_constant(SNAME("vseparation"));
		Size2 bems = bottom_editor->get_combined_minimum_size();
		//bems.width += get_constant("item_margin", "Tree");
		ms.height += bems.height;
		ms.width = MAX(ms.width, bems.width);
	}

	return ms;
}

void EditorProperty::emit_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field, bool p_changing) {
	Variant args[4] = { p_property, p_value, p_field, p_changing };
	const Variant *argptrs[4] = { &args[0], &args[1], &args[2], &args[3] };

	cache[p_property] = p_value;
	emit_signal(SNAME("property_changed"), (const Variant **)argptrs, 4);
}

void EditorProperty::_notification(int p_what) {
	if (p_what == NOTIFICATION_SORT_CHILDREN) {
		Size2 size = get_size();
		Rect2 rect;
		Rect2 bottom_rect;

		right_child_rect = Rect2();
		bottom_child_rect = Rect2();

		{
			int child_room = size.width * (1.0 - split_ratio);
			Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Tree"));
			int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Tree"));
			int height = font->get_height(font_size);
			bool no_children = true;

			//compute room needed
			for (int i = 0; i < get_child_count(); i++) {
				Control *c = Object::cast_to<Control>(get_child(i));
				if (!c) {
					continue;
				}
				if (c->is_set_as_top_level()) {
					continue;
				}
				if (c == bottom_editor) {
					continue;
				}

				Size2 minsize = c->get_combined_minimum_size();
				child_room = MAX(child_room, minsize.width);
				height = MAX(height, minsize.height);
				no_children = false;
			}

			if (no_children) {
				text_size = size.width;
				rect = Rect2(size.width - 1, 0, 1, height);
			} else {
				text_size = MAX(0, size.width - (child_room + 4 * EDSCALE));
				if (is_layout_rtl()) {
					rect = Rect2(1, 0, child_room, height);
				} else {
					rect = Rect2(size.width - child_room, 0, child_room, height);
				}
			}

			if (bottom_editor) {
				int m = 0; //get_constant("item_margin", "Tree");

				bottom_rect = Rect2(m, rect.size.height + get_theme_constant(SNAME("vseparation")), size.width - m, bottom_editor->get_combined_minimum_size().height);
			}

			if (keying) {
				Ref<Texture2D> key;

				if (use_keying_next()) {
					key = get_theme_icon(SNAME("KeyNext"), SNAME("EditorIcons"));
				} else {
					key = get_theme_icon(SNAME("Key"), SNAME("EditorIcons"));
				}

				rect.size.x -= key->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));
				if (is_layout_rtl()) {
					rect.position.x += key->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));
				}

				if (no_children) {
					text_size -= key->get_width() + 4 * EDSCALE;
				}
			}

			if (deletable) {
				Ref<Texture2D> close;

				close = get_theme_icon(SNAME("Close"), SNAME("EditorIcons"));

				rect.size.x -= close->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));

				if (is_layout_rtl()) {
					rect.position.x += close->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree"));
				}

				if (no_children) {
					text_size -= close->get_width() + 4 * EDSCALE;
				}
			}
		}

		//set children
		for (int i = 0; i < get_child_count(); i++) {
			Control *c = Object::cast_to<Control>(get_child(i));
			if (!c) {
				continue;
			}
			if (c->is_set_as_top_level()) {
				continue;
			}
			if (c == bottom_editor) {
				continue;
			}

			fit_child_in_rect(c, rect);
			right_child_rect = rect;
		}

		if (bottom_editor) {
			fit_child_in_rect(bottom_editor, bottom_rect);
			bottom_child_rect = bottom_rect;
		}

		update(); //need to redraw text
	}

	if (p_what == NOTIFICATION_DRAW) {
		Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Tree"));
		int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Tree"));
		Color dark_color = get_theme_color(SNAME("dark_color_2"), SNAME("Editor"));
		bool rtl = is_layout_rtl();

		Size2 size = get_size();
		if (bottom_editor) {
			size.height = bottom_editor->get_offset(SIDE_TOP);
		} else if (label_reference) {
			size.height = label_reference->get_size().height;
		}

		Ref<StyleBox> sb;
		if (selected) {
			sb = get_theme_stylebox(SNAME("bg_selected"));
		} else {
			sb = get_theme_stylebox(SNAME("bg"));
		}

		draw_style_box(sb, Rect2(Vector2(), size));

		if (draw_top_bg && right_child_rect != Rect2()) {
			draw_rect(right_child_rect, dark_color);
		}
		if (bottom_child_rect != Rect2()) {
			draw_rect(bottom_child_rect, dark_color);
		}

		Color color;
		if (draw_red) {
			color = get_theme_color(SNAME("error_color"));
		} else {
			color = get_theme_color(SNAME("property_color"));
		}
		if (label.find(".") != -1) {
			color.a = 0.5; //this should be un-hacked honestly, as it's used for editor overrides
		}

		int ofs = get_theme_constant(SNAME("font_offset"));
		int text_limit = text_size;

		if (checkable) {
			Ref<Texture2D> checkbox;
			if (checked) {
				checkbox = get_theme_icon(SNAME("GuiChecked"), SNAME("EditorIcons"));
			} else {
				checkbox = get_theme_icon(SNAME("GuiUnchecked"), SNAME("EditorIcons"));
			}

			Color color2(1, 1, 1);
			if (check_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}
			check_rect = Rect2(ofs, ((size.height - checkbox->get_height()) / 2), checkbox->get_width(), checkbox->get_height());
			if (rtl) {
				draw_texture(checkbox, Vector2(size.width - check_rect.position.x - checkbox->get_width(), check_rect.position.y), color2);
			} else {
				draw_texture(checkbox, check_rect.position, color2);
			}
			ofs += get_theme_constant(SNAME("hseparator"), SNAME("Tree")) + checkbox->get_width() + get_theme_constant(SNAME("hseparation"), SNAME("CheckBox"));
			text_limit -= ofs;
		} else {
			check_rect = Rect2();
		}

		if (can_revert) {
			Ref<Texture2D> reload_icon = get_theme_icon(SNAME("ReloadSmall"), SNAME("EditorIcons"));
			text_limit -= reload_icon->get_width() + get_theme_constant(SNAME("hseparator"), SNAME("Tree")) * 2;
			revert_rect = Rect2(text_limit + get_theme_constant(SNAME("hseparator"), SNAME("Tree")), (size.height - reload_icon->get_height()) / 2, reload_icon->get_width(), reload_icon->get_height());

			Color color2(1, 1, 1);
			if (revert_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}
			if (rtl) {
				draw_texture(reload_icon, Vector2(size.width - revert_rect.position.x - reload_icon->get_width(), revert_rect.position.y), color2);
			} else {
				draw_texture(reload_icon, revert_rect.position, color2);
			}
		} else {
			revert_rect = Rect2();
		}

		int v_ofs = (size.height - font->get_height(font_size)) / 2;
		if (rtl) {
			draw_string(font, Point2(size.width - ofs - text_limit, v_ofs + font->get_ascent(font_size)), label, HALIGN_RIGHT, text_limit, font_size, color);
		} else {
			draw_string(font, Point2(ofs, v_ofs + font->get_ascent(font_size)), label, HALIGN_LEFT, text_limit, font_size, color);
		}

		if (keying) {
			Ref<Texture2D> key;

			if (use_keying_next()) {
				key = get_theme_icon(SNAME("KeyNext"), SNAME("EditorIcons"));
			} else {
				key = get_theme_icon(SNAME("Key"), SNAME("EditorIcons"));
			}

			ofs = size.width - key->get_width() - get_theme_constant(SNAME("hseparator"), SNAME("Tree"));

			Color color2(1, 1, 1);
			if (keying_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}
			keying_rect = Rect2(ofs, ((size.height - key->get_height()) / 2), key->get_width(), key->get_height());
			if (rtl) {
				draw_texture(key, Vector2(size.width - keying_rect.position.x - key->get_width(), keying_rect.position.y), color2);
			} else {
				draw_texture(key, keying_rect.position, color2);
			}

		} else {
			keying_rect = Rect2();
		}

		if (deletable) {
			Ref<Texture2D> close;

			close = get_theme_icon(SNAME("Close"), SNAME("EditorIcons"));

			ofs = size.width - close->get_width() - get_theme_constant(SNAME("hseparator"), SNAME("Tree"));

			Color color2(1, 1, 1);
			if (delete_hover) {
				color2.r *= 1.2;
				color2.g *= 1.2;
				color2.b *= 1.2;
			}
			delete_rect = Rect2(ofs, ((size.height - close->get_height()) / 2), close->get_width(), close->get_height());
			if (rtl) {
				draw_texture(close, Vector2(size.width - delete_rect.position.x - close->get_width(), delete_rect.position.y), color2);
			} else {
				draw_texture(close, delete_rect.position, color2);
			}
		} else {
			delete_rect = Rect2();
		}
	}
}

void EditorProperty::set_label(const String &p_label) {
	label = p_label;
	update();
}

String EditorProperty::get_label() const {
	return label;
}

Object *EditorProperty::get_edited_object() {
	return object;
}

StringName EditorProperty::get_edited_property() {
	return property;
}

void EditorProperty::update_property() {
	if (get_script_instance()) {
		get_script_instance()->call("_update_property");
	}
}

void EditorProperty::set_read_only(bool p_read_only) {
	read_only = p_read_only;
}

bool EditorProperty::is_read_only() const {
	return read_only;
}

bool EditorPropertyRevert::may_node_be_in_instance(Node *p_node) {
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();

	bool might_be = false;
	Node *node = p_node;

	while (node) {
		if (node == edited_scene) {
			if (node->get_scene_inherited_state().is_valid()) {
				might_be = true;
				break;
			}
			might_be = false;
			break;
		}
		if (node->get_scene_instance_state().is_valid()) {
			might_be = true;
			break;
		}
		node = node->get_owner();
	}

	return might_be; // or might not be
}

bool EditorPropertyRevert::get_instantiated_node_original_property(Node *p_node, const StringName &p_prop, Variant &value, bool p_check_class_default) {
	Node *node = p_node;
	Node *orig = node;

	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();

	bool found = false;

	while (node) {
		Ref<SceneState> ss;

		if (node == edited_scene) {
			ss = node->get_scene_inherited_state();

		} else {
			ss = node->get_scene_instance_state();
		}

		if (ss.is_valid()) {
			NodePath np = node->get_path_to(orig);
			int node_idx = ss->find_node_by_path(np);
			if (node_idx >= 0) {
				bool lfound = false;
				Variant lvar;
				lvar = ss->get_property_value(node_idx, p_prop, lfound);
				if (lfound) {
					found = true;
					value = lvar;
				}
			}
		}
		if (node == edited_scene) {
			//just in case
			break;
		}
		node = node->get_owner();
	}

	if (p_check_class_default && !found && p_node) {
		//if not found, try default class value
		Variant attempt = ClassDB::class_get_default_property_value(p_node->get_class_name(), p_prop);
		if (attempt.get_type() != Variant::NIL) {
			found = true;
			value = attempt;
		}
	}

	return found;
}

bool EditorPropertyRevert::is_node_property_different(Node *p_node, const Variant &p_current, const Variant &p_orig) {
	// this is a pretty difficult function, because a property may not be saved but may have
	// the flag to not save if one or if zero

	//make sure there is an actual state
	{
		Node *node = p_node;
		if (!node) {
			return false;
		}

		Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
		bool found_state = false;

		while (node) {
			Ref<SceneState> ss;

			if (node == edited_scene) {
				ss = node->get_scene_inherited_state();

			} else {
				ss = node->get_scene_instance_state();
			}

			if (ss.is_valid()) {
				found_state = true;
				break;
			}
			if (node == edited_scene) {
				//just in case
				break;
			}
			node = node->get_owner();
		}

		if (!found_state) {
			return false; //pointless to check if we are not comparing against anything.
		}
	}

	return is_property_value_different(p_current, p_orig);
}

bool EditorPropertyRevert::is_property_value_different(const Variant &p_a, const Variant &p_b) {
	if (p_a.get_type() == Variant::FLOAT && p_b.get_type() == Variant::FLOAT) {
		//this must be done because, as some scenes save as text, there might be a tiny difference in floats due to numerical error
		return !Math::is_equal_approx((float)p_a, (float)p_b);
	} else {
		return p_a != p_b;
	}
}

Variant EditorPropertyRevert::get_property_revert_value(Object *p_object, const StringName &p_property) {
	// If the object implements property_can_revert, rely on that completely
	// (i.e. don't then try to revert to default value - the property_get_revert implementation
	// can do that if so desired)
	if (p_object->has_method("property_can_revert") && p_object->call("property_can_revert", p_property)) {
		return p_object->call("property_get_revert", p_property);
	}

	Ref<Script> scr = p_object->get_script();
	Node *node = Object::cast_to<Node>(p_object);
	if (node && EditorPropertyRevert::may_node_be_in_instance(node)) {
		//if this node is an instance or inherits, but it has a script attached which is unrelated
		//to the one set for the parent and also has a default value for the property, consider that
		//has precedence over the value from the parent, because that is an explicit source of defaults
		//closer in the tree to the current node
		bool ignore_parent = false;
		if (scr.is_valid()) {
			Variant sorig;
			if (EditorPropertyRevert::get_instantiated_node_original_property(node, "script", sorig) && !scr->inherits_script(sorig)) {
				Variant dummy;
				if (scr->get_property_default_value(p_property, dummy)) {
					ignore_parent = true;
				}
			}
		}

		if (!ignore_parent) {
			//check for difference including instantiation
			Variant vorig;
			if (EditorPropertyRevert::get_instantiated_node_original_property(node, p_property, vorig, false)) {
				return vorig;
			}
		}
	}

	if (scr.is_valid()) {
		Variant orig_value;
		if (scr->get_property_default_value(p_property, orig_value)) {
			return orig_value;
		}
	}

	//report default class value instead
	return ClassDB::class_get_default_property_value(p_object->get_class_name(), p_property);
}

bool EditorPropertyRevert::can_property_revert(Object *p_object, const StringName &p_property) {
	Variant revert_value = EditorPropertyRevert::get_property_revert_value(p_object, p_property);
	if (revert_value.get_type() == Variant::NIL) {
		return false;
	}
	Variant current_value = p_object->get(p_property);
	return EditorPropertyRevert::is_property_value_different(current_value, revert_value);
}

void EditorProperty::update_reload_status() {
	if (property == StringName()) {
		return; //no property, so nothing to do
	}

	bool has_reload = EditorPropertyRevert::can_property_revert(object, property);

	if (has_reload != can_revert) {
		can_revert = has_reload;
		update();
	}
}

bool EditorProperty::use_keying_next() const {
	List<PropertyInfo> plist;
	object->get_property_list(&plist, true);

	for (List<PropertyInfo>::Element *I = plist.front(); I; I = I->next()) {
		PropertyInfo &p = I->get();

		if (p.name == property) {
			return (p.usage & PROPERTY_USAGE_KEYING_INCREMENTS);
		}
	}

	return false;
}

void EditorProperty::set_checkable(bool p_checkable) {
	checkable = p_checkable;
	update();
	queue_sort();
}

bool EditorProperty::is_checkable() const {
	return checkable;
}

void EditorProperty::set_checked(bool p_checked) {
	checked = p_checked;
	update();
}

bool EditorProperty::is_checked() const {
	return checked;
}

void EditorProperty::set_draw_red(bool p_draw_red) {
	draw_red = p_draw_red;
	update();
}

void EditorProperty::set_keying(bool p_keying) {
	keying = p_keying;
	update();
	queue_sort();
}

void EditorProperty::set_deletable(bool p_deletable) {
	deletable = p_deletable;
	update();
	queue_sort();
}

bool EditorProperty::is_deletable() const {
	return deletable;
}

bool EditorProperty::is_keying() const {
	return keying;
}

bool EditorProperty::is_draw_red() const {
	return draw_red;
}

void EditorProperty::_focusable_focused(int p_index) {
	if (!selectable) {
		return;
	}
	bool already_selected = selected;
	selected = true;
	selected_focusable = p_index;
	update();
	if (!already_selected && selected) {
		emit_signal(SNAME("selected"), property, selected_focusable);
	}
}

void EditorProperty::add_focusable(Control *p_control) {
	p_control->connect("focus_entered", callable_mp(this, &EditorProperty::_focusable_focused), varray(focusables.size()));
	focusables.push_back(p_control);
}

void EditorProperty::select(int p_focusable) {
	bool already_selected = selected;

	if (p_focusable >= 0) {
		ERR_FAIL_INDEX(p_focusable, focusables.size());
		focusables[p_focusable]->grab_focus();
	} else {
		selected = true;
		update();
	}

	if (!already_selected && selected) {
		emit_signal(SNAME("selected"), property, selected_focusable);
	}
}

void EditorProperty::deselect() {
	selected = false;
	selected_focusable = -1;
	update();
}

bool EditorProperty::is_selected() const {
	return selected;
}

void EditorProperty::_gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (property == StringName()) {
		return;
	}

	Ref<InputEventMouse> me = p_event;

	if (me.is_valid()) {
		Vector2 mpos = me->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}
		bool button_left = me->get_button_mask() & MOUSE_BUTTON_MASK_LEFT;

		bool new_keying_hover = keying_rect.has_point(mpos) && !button_left;
		if (new_keying_hover != keying_hover) {
			keying_hover = new_keying_hover;
			update();
		}

		bool new_delete_hover = delete_rect.has_point(mpos) && !button_left;
		if (new_delete_hover != delete_hover) {
			delete_hover = new_delete_hover;
			update();
		}

		bool new_revert_hover = revert_rect.has_point(mpos) && !button_left;
		if (new_revert_hover != revert_hover) {
			revert_hover = new_revert_hover;
			update();
		}

		bool new_check_hover = check_rect.has_point(mpos) && !button_left;
		if (new_check_hover != check_hover) {
			check_hover = new_check_hover;
			update();
		}
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		Vector2 mpos = mb->get_position();
		if (is_layout_rtl()) {
			mpos.x = get_size().x - mpos.x;
		}

		if (!selected && selectable) {
			selected = true;
			emit_signal(SNAME("selected"), property, -1);
			update();
		}

		if (keying_rect.has_point(mpos)) {
			emit_signal(SNAME("property_keyed"), property, use_keying_next());

			if (use_keying_next()) {
				if (property == "frame_coords" && (object->is_class("Sprite2D") || object->is_class("Sprite3D"))) {
					Vector2i new_coords = object->get(property);
					new_coords.x++;
					if (new_coords.x >= object->get("hframes").operator int64_t()) {
						new_coords.x = 0;
						new_coords.y++;
					}

					call_deferred(SNAME("emit_changed"), property, new_coords, "", false);
				} else {
					call_deferred(SNAME("emit_changed"), property, object->get(property).operator int64_t() + 1, "", false);
				}

				call_deferred(SNAME("_update_property"));
			}
		}
		if (delete_rect.has_point(mpos)) {
			emit_signal(SNAME("property_deleted"), property);
		}

		if (revert_rect.has_point(mpos)) {
			Variant revert_value = EditorPropertyRevert::get_property_revert_value(object, property);
			emit_changed(property, revert_value);
			update_property();
		}

		if (check_rect.has_point(mpos)) {
			checked = !checked;
			update();
			emit_signal(SNAME("property_checked"), property, checked);
		}
	}
}

void EditorProperty::set_label_reference(Control *p_control) {
	label_reference = p_control;
}

void EditorProperty::set_bottom_editor(Control *p_control) {
	bottom_editor = p_control;
}

bool EditorProperty::is_cache_valid() const {
	if (object) {
		for (Map<StringName, Variant>::Element *E = cache.front(); E; E = E->next()) {
			bool valid;
			Variant value = object->get(E->key(), &valid);
			if (!valid || value != E->get()) {
				return false;
			}
		}
	}
	return true;
}
void EditorProperty::update_cache() {
	cache.clear();
	if (object && property != StringName()) {
		bool valid;
		Variant value = object->get(property, &valid);
		if (valid) {
			cache[property] = value;
		}
	}
}
Variant EditorProperty::get_drag_data(const Point2 &p_point) {
	if (property == StringName()) {
		return Variant();
	}

	Dictionary dp;
	dp["type"] = "obj_property";
	dp["object"] = object;
	dp["property"] = property;
	dp["value"] = object->get(property);

	Label *label = memnew(Label);
	label->set_text(property);
	set_drag_preview(label);
	return dp;
}

void EditorProperty::set_use_folding(bool p_use_folding) {
	use_folding = p_use_folding;
}

bool EditorProperty::is_using_folding() const {
	return use_folding;
}

void EditorProperty::expand_all_folding() {
}

void EditorProperty::collapse_all_folding() {
}

void EditorProperty::set_selectable(bool p_selectable) {
	selectable = p_selectable;
}

bool EditorProperty::is_selectable() const {
	return selectable;
}

void EditorProperty::set_name_split_ratio(float p_ratio) {
	split_ratio = p_ratio;
}

float EditorProperty::get_name_split_ratio() const {
	return split_ratio;
}

void EditorProperty::set_object_and_property(Object *p_object, const StringName &p_property) {
	object = p_object;
	property = p_property;
}

Control *EditorProperty::make_custom_tooltip(const String &p_text) const {
	tooltip_text = p_text;
	EditorHelpBit *help_bit = memnew(EditorHelpBit);
	//help_bit->add_theme_style_override("panel", get_theme_stylebox(SNAME("panel"), SNAME("TooltipPanel")));
	help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

	String text;
	PackedStringArray slices = p_text.split("::", false);
	if (!slices.is_empty()) {
		String property_name = slices[0].strip_edges();
		text = TTR("Property:") + " [u][b]" + property_name + "[/b][/u]";

		if (slices.size() > 1) {
			String property_doc = slices[1].strip_edges();
			if (property_name != property_doc) {
				text += "\n" + property_doc;
			}
		}
		help_bit->set_text(text);
	}

	return help_bit;
}

String EditorProperty::get_tooltip_text() const {
	return tooltip_text;
}

void EditorProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_label", "text"), &EditorProperty::set_label);
	ClassDB::bind_method(D_METHOD("get_label"), &EditorProperty::get_label);

	ClassDB::bind_method(D_METHOD("set_read_only", "read_only"), &EditorProperty::set_read_only);
	ClassDB::bind_method(D_METHOD("is_read_only"), &EditorProperty::is_read_only);

	ClassDB::bind_method(D_METHOD("set_checkable", "checkable"), &EditorProperty::set_checkable);
	ClassDB::bind_method(D_METHOD("is_checkable"), &EditorProperty::is_checkable);

	ClassDB::bind_method(D_METHOD("set_checked", "checked"), &EditorProperty::set_checked);
	ClassDB::bind_method(D_METHOD("is_checked"), &EditorProperty::is_checked);

	ClassDB::bind_method(D_METHOD("set_draw_red", "draw_red"), &EditorProperty::set_draw_red);
	ClassDB::bind_method(D_METHOD("is_draw_red"), &EditorProperty::is_draw_red);

	ClassDB::bind_method(D_METHOD("set_keying", "keying"), &EditorProperty::set_keying);
	ClassDB::bind_method(D_METHOD("is_keying"), &EditorProperty::is_keying);

	ClassDB::bind_method(D_METHOD("set_deletable", "deletable"), &EditorProperty::set_deletable);
	ClassDB::bind_method(D_METHOD("is_deletable"), &EditorProperty::is_deletable);

	ClassDB::bind_method(D_METHOD("get_edited_property"), &EditorProperty::get_edited_property);
	ClassDB::bind_method(D_METHOD("get_edited_object"), &EditorProperty::get_edited_object);

	ClassDB::bind_method(D_METHOD("_gui_input"), &EditorProperty::_gui_input);

	ClassDB::bind_method(D_METHOD("get_tooltip_text"), &EditorProperty::get_tooltip_text);

	ClassDB::bind_method(D_METHOD("add_focusable", "control"), &EditorProperty::add_focusable);
	ClassDB::bind_method(D_METHOD("set_bottom_editor", "editor"), &EditorProperty::set_bottom_editor);

	ClassDB::bind_method(D_METHOD("emit_changed", "property", "value", "field", "changing"), &EditorProperty::emit_changed, DEFVAL(StringName()), DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "read_only"), "set_read_only", "is_read_only");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checkable"), "set_checkable", "is_checkable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "checked"), "set_checked", "is_checked");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_red"), "set_draw_red", "is_draw_red");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keying"), "set_keying", "is_keying");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deletable"), "set_deletable", "is_deletable");
	ADD_SIGNAL(MethodInfo("property_changed", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("multiple_properties_changed", PropertyInfo(Variant::PACKED_STRING_ARRAY, "properties"), PropertyInfo(Variant::ARRAY, "value")));
	ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(Variant::STRING_NAME, "property")));
	ADD_SIGNAL(MethodInfo("property_deleted", PropertyInfo(Variant::STRING_NAME, "property")));
	ADD_SIGNAL(MethodInfo("property_keyed_with_value", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::NIL, "value", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("property_checked", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::STRING, "bool")));
	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
	ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(Variant::STRING_NAME, "property"), PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "focusable_idx")));

	BIND_VMETHOD(MethodInfo("_update_property"));
}

EditorProperty::EditorProperty() {
	draw_top_bg = true;
	object = nullptr;
	split_ratio = 0.5;
	selectable = true;
	text_size = 0;
	read_only = false;
	checkable = false;
	checked = false;
	draw_red = false;
	keying = false;
	deletable = false;
	keying_hover = false;
	revert_hover = false;
	check_hover = false;
	can_revert = false;
	use_folding = false;
	property_usage = 0;
	selected = false;
	selected_focusable = -1;
	label_reference = nullptr;
	bottom_editor = nullptr;
	delete_hover = false;
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorPlugin::add_custom_control(Control *control) {
	AddedEditor ae;
	ae.property_editor = control;
	added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor(const String &p_for_property, Control *p_prop) {
	ERR_FAIL_COND(Object::cast_to<EditorProperty>(p_prop) == nullptr);

	AddedEditor ae;
	ae.properties.push_back(p_for_property);
	ae.property_editor = p_prop;
	added_editors.push_back(ae);
}

void EditorInspectorPlugin::add_property_editor_for_multiple_properties(const String &p_label, const Vector<String> &p_properties, Control *p_prop) {
	AddedEditor ae;
	ae.properties = p_properties;
	ae.property_editor = p_prop;
	ae.label = p_label;
	added_editors.push_back(ae);
}

bool EditorInspectorPlugin::can_handle(Object *p_object) {
	if (get_script_instance()) {
		return get_script_instance()->call("_can_handle", p_object);
	}
	return false;
}

void EditorInspectorPlugin::parse_begin(Object *p_object) {
	if (get_script_instance()) {
		get_script_instance()->call("_parse_begin", p_object);
	}
}

void EditorInspectorPlugin::parse_category(Object *p_object, const String &p_parse_category) {
	if (get_script_instance()) {
		get_script_instance()->call("_parse_category", p_object, p_parse_category);
	}
}

bool EditorInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide) {
	if (get_script_instance()) {
		Variant arg[6] = {
			p_object, p_type, p_path, p_hint, p_hint_text, p_usage
		};
		const Variant *argptr[6] = {
			&arg[0], &arg[1], &arg[2], &arg[3], &arg[4], &arg[5]
		};

		Callable::CallError err;
		return get_script_instance()->call("_parse_property", (const Variant **)&argptr, 6, err);
	}
	return false;
}

void EditorInspectorPlugin::parse_end() {
	if (get_script_instance()) {
		get_script_instance()->call("_parse_end");
	}
}

void EditorInspectorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_custom_control", "control"), &EditorInspectorPlugin::add_custom_control);
	ClassDB::bind_method(D_METHOD("add_property_editor", "property", "editor"), &EditorInspectorPlugin::add_property_editor);
	ClassDB::bind_method(D_METHOD("add_property_editor_for_multiple_properties", "label", "properties", "editor"), &EditorInspectorPlugin::add_property_editor_for_multiple_properties);

	BIND_VMETHOD(MethodInfo(Variant::BOOL, "_can_handle", PropertyInfo(Variant::OBJECT, "object")));
	BIND_VMETHOD(MethodInfo(Variant::NIL, "_parse_begin"));
	BIND_VMETHOD(MethodInfo(Variant::NIL, "_parse_category", PropertyInfo(Variant::STRING, "category")));
	BIND_VMETHOD(MethodInfo(Variant::BOOL, "_parse_property", PropertyInfo(Variant::INT, "type"), PropertyInfo(Variant::STRING, "path"), PropertyInfo(Variant::INT, "hint"), PropertyInfo(Variant::STRING, "hint_text"), PropertyInfo(Variant::INT, "usage")));
	BIND_VMETHOD(MethodInfo(Variant::NIL, "_parse_end"));
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorCategory::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		Ref<StyleBox> sb = get_theme_stylebox(SNAME("prop_category_style"), SNAME("Editor"));

		draw_style_box(sb, Rect2(Vector2(), get_size()));

		Ref<Font> font = get_theme_font(SNAME("bold"), SNAME("EditorFonts"));
		int font_size = get_theme_font_size(SNAME("bold_size"), SNAME("EditorFonts"));

		int hs = get_theme_constant(SNAME("hseparation"), SNAME("Tree"));

		int w = font->get_string_size(label, font_size).width;
		if (icon.is_valid()) {
			w += hs + icon->get_width();
		}

		int ofs = (get_size().width - w) / 2;

		if (icon.is_valid()) {
			draw_texture(icon, Point2(ofs, (get_size().height - icon->get_height()) / 2).floor());
			ofs += hs + icon->get_width();
		}

		Color color = get_theme_color(SNAME("font_color"), SNAME("Tree"));
		draw_string(font, Point2(ofs, font->get_ascent(font_size) + (get_size().height - font->get_height(font_size)) / 2).floor(), label, HALIGN_LEFT, get_size().width, font_size, color);
	}
}

Control *EditorInspectorCategory::make_custom_tooltip(const String &p_text) const {
	tooltip_text = p_text;
	EditorHelpBit *help_bit = memnew(EditorHelpBit);
	help_bit->add_theme_style_override("panel", get_theme_stylebox(SNAME("panel"), SNAME("TooltipPanel")));
	help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

	PackedStringArray slices = p_text.split("::", false);
	if (!slices.is_empty()) {
		String property_name = slices[0].strip_edges();
		String text = "[u][b]" + property_name + "[/b][/u]";

		if (slices.size() > 1) {
			String property_doc = slices[1].strip_edges();
			if (property_name != property_doc) {
				text += "\n" + property_doc;
			}
		}
		help_bit->set_text(text); //hack so it uses proper theme once inside scene
	}

	return help_bit;
}

Size2 EditorInspectorCategory::get_minimum_size() const {
	Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Tree"));
	int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Tree"));

	Size2 ms;
	ms.width = 1;
	ms.height = font->get_height(font_size);
	if (icon.is_valid()) {
		ms.height = MAX(icon->get_height(), ms.height);
	}
	ms.height += get_theme_constant(SNAME("vseparation"), SNAME("Tree"));

	return ms;
}

void EditorInspectorCategory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tooltip_text"), &EditorInspectorCategory::get_tooltip_text);
}

String EditorInspectorCategory::get_tooltip_text() const {
	return tooltip_text;
}

EditorInspectorCategory::EditorInspectorCategory() {
}

////////////////////////////////////////////////
////////////////////////////////////////////////

void EditorInspectorSection::_test_unfold() {
	if (!vbox_added) {
		add_child(vbox);
		vbox_added = true;
	}
}

void EditorInspectorSection::_notification(int p_what) {
	if (p_what == NOTIFICATION_SORT_CHILDREN) {
		Ref<Font> font = get_theme_font(SNAME("bold"), SNAME("EditorFonts"));
		int font_size = get_theme_font_size(SNAME("bold_size"), SNAME("EditorFonts"));

		Ref<Texture2D> arrow;

		if (foldable) {
			if (object->editor_is_section_unfolded(section)) {
				arrow = get_theme_icon(SNAME("arrow"), SNAME("Tree"));
			} else {
				if (is_layout_rtl()) {
					arrow = get_theme_icon(SNAME("arrow_collapsed_mirrored"), SNAME("Tree"));
				} else {
					arrow = get_theme_icon(SNAME("arrow_collapsed"), SNAME("Tree"));
				}
			}
		}

		Size2 size = get_size();
		Point2 offset;
		Rect2 rect;
		offset.y = font->get_height(font_size);
		if (arrow.is_valid()) {
			offset.y = MAX(offset.y, arrow->get_height());
		}

		offset.y += get_theme_constant(SNAME("vseparation"), SNAME("Tree"));
		if (is_layout_rtl()) {
			rect = Rect2(offset, size - offset - Vector2(get_theme_constant(SNAME("inspector_margin"), SNAME("Editor")), 0));
		} else {
			offset.x += get_theme_constant(SNAME("inspector_margin"), SNAME("Editor"));
			rect = Rect2(offset, size - offset);
		}

		//set children
		for (int i = 0; i < get_child_count(); i++) {
			Control *c = Object::cast_to<Control>(get_child(i));
			if (!c) {
				continue;
			}
			if (c->is_set_as_top_level()) {
				continue;
			}
			if (!c->is_visible_in_tree()) {
				continue;
			}

			fit_child_in_rect(c, rect);
		}

		update(); //need to redraw text
	}

	if (p_what == NOTIFICATION_DRAW) {
		Ref<Texture2D> arrow;
		bool rtl = is_layout_rtl();

		if (foldable) {
			if (object->editor_is_section_unfolded(section)) {
				arrow = get_theme_icon(SNAME("arrow"), SNAME("Tree"));
			} else {
				if (is_layout_rtl()) {
					arrow = get_theme_icon(SNAME("arrow_collapsed_mirrored"), SNAME("Tree"));
				} else {
					arrow = get_theme_icon(SNAME("arrow_collapsed"), SNAME("Tree"));
				}
			}
		}

		Ref<Font> font = get_theme_font(SNAME("bold"), SNAME("EditorFonts"));
		int font_size = get_theme_font_size(SNAME("bold_size"), SNAME("EditorFonts"));

		int h = font->get_height(font_size);
		if (arrow.is_valid()) {
			h = MAX(h, arrow->get_height());
		}
		h += get_theme_constant(SNAME("vseparation"), SNAME("Tree"));

		Color c = bg_color;
		c.a *= 0.4;
		draw_rect(Rect2(Vector2(), Vector2(get_size().width, h)), c);

		const int arrow_margin = 2;
		const int arrow_width = arrow.is_valid() ? arrow->get_width() : 0;
		Color color = get_theme_color(SNAME("font_color"));
		float text_width = get_size().width - Math::round(arrow_width + arrow_margin * EDSCALE);
		draw_string(font, Point2(rtl ? 0 : Math::round(arrow_width + arrow_margin * EDSCALE), font->get_ascent(font_size) + (h - font->get_height(font_size)) / 2).floor(), label, rtl ? HALIGN_RIGHT : HALIGN_LEFT, text_width, font_size, color);

		if (arrow.is_valid()) {
			if (rtl) {
				draw_texture(arrow, Point2(get_size().width - arrow->get_width() - Math::round(arrow_margin * EDSCALE), (h - arrow->get_height()) / 2).floor());
			} else {
				draw_texture(arrow, Point2(Math::round(arrow_margin * EDSCALE), (h - arrow->get_height()) / 2).floor());
			}
		}

		if (dropping && !vbox->is_visible_in_tree()) {
			Color accent_color = get_theme_color(SNAME("accent_color"), SNAME("Editor"));
			draw_rect(Rect2(Point2(), get_size()), accent_color, false);
		}
	}

	if (p_what == NOTIFICATION_DRAG_BEGIN) {
		Dictionary dd = get_viewport()->gui_get_drag_data();

		// Only allow dropping if the section contains properties which can take the dragged data.
		bool children_can_drop = false;
		for (int child_idx = 0; child_idx < vbox->get_child_count(); child_idx++) {
			Control *editor_property = Object::cast_to<Control>(vbox->get_child(child_idx));

			// Test can_drop_data and can_drop_data_fw, since can_drop_data only works if set up with forwarding or if script attached.
			if (editor_property && (editor_property->can_drop_data(Point2(), dd) || editor_property->call("_can_drop_data_fw", Point2(), dd, this))) {
				children_can_drop = true;
				break;
			}
		}

		dropping = children_can_drop;
		update();
	}

	if (p_what == NOTIFICATION_DRAG_END) {
		dropping = false;
		update();
	}

	if (p_what == NOTIFICATION_MOUSE_ENTER) {
		if (dropping) {
			dropping_unfold_timer->start();
		}
	}

	if (p_what == NOTIFICATION_MOUSE_EXIT) {
		if (dropping) {
			dropping_unfold_timer->stop();
		}
	}
}

Size2 EditorInspectorSection::get_minimum_size() const {
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}
		if (c->is_set_as_top_level()) {
			continue;
		}
		if (!c->is_visible()) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();
		ms.width = MAX(ms.width, minsize.width);
		ms.height = MAX(ms.height, minsize.height);
	}

	Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Tree"));
	int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Tree"));
	ms.height += font->get_height(font_size) + get_theme_constant(SNAME("vseparation"), SNAME("Tree"));
	ms.width += get_theme_constant(SNAME("inspector_margin"), SNAME("Editor"));

	return ms;
}

void EditorInspectorSection::setup(const String &p_section, const String &p_label, Object *p_object, const Color &p_bg_color, bool p_foldable) {
	section = p_section;
	label = p_label;
	object = p_object;
	bg_color = p_bg_color;
	foldable = p_foldable;

	if (!foldable && !vbox_added) {
		add_child(vbox);
		vbox_added = true;
	}

	if (foldable) {
		_test_unfold();
		if (object->editor_is_section_unfolded(section)) {
			vbox->show();
		} else {
			vbox->hide();
		}
	}
}

void EditorInspectorSection::_gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!foldable) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
		Ref<Font> font = get_theme_font(SNAME("font"), SNAME("Tree"));
		int font_size = get_theme_font_size(SNAME("font_size"), SNAME("Tree"));
		if (mb->get_position().y > font->get_height(font_size)) { //clicked outside
			return;
		}

		bool should_unfold = !object->editor_is_section_unfolded(section);
		if (should_unfold) {
			unfold();
		} else {
			fold();
		}
	}
}

VBoxContainer *EditorInspectorSection::get_vbox() {
	return vbox;
}

void EditorInspectorSection::unfold() {
	if (!foldable) {
		return;
	}

	_test_unfold();

	object->editor_set_section_unfold(section, true);
	vbox->show();
	update();
}

void EditorInspectorSection::fold() {
	if (!foldable) {
		return;
	}

	if (!vbox_added) {
		return; //kinda pointless
	}

	object->editor_set_section_unfold(section, false);
	vbox->hide();
	update();
}

void EditorInspectorSection::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "section", "label", "object", "bg_color", "foldable"), &EditorInspectorSection::setup);
	ClassDB::bind_method(D_METHOD("get_vbox"), &EditorInspectorSection::get_vbox);
	ClassDB::bind_method(D_METHOD("unfold"), &EditorInspectorSection::unfold);
	ClassDB::bind_method(D_METHOD("fold"), &EditorInspectorSection::fold);
	ClassDB::bind_method(D_METHOD("_gui_input"), &EditorInspectorSection::_gui_input);
}

EditorInspectorSection::EditorInspectorSection() {
	object = nullptr;
	foldable = false;
	vbox = memnew(VBoxContainer);
	vbox_added = false;

	dropping = false;
	dropping_unfold_timer = memnew(Timer);
	dropping_unfold_timer->set_wait_time(0.6);
	dropping_unfold_timer->set_one_shot(true);
	add_child(dropping_unfold_timer);
	dropping_unfold_timer->connect("timeout", callable_mp(this, &EditorInspectorSection::unfold));
}

EditorInspectorSection::~EditorInspectorSection() {
	if (!vbox_added) {
		memdelete(vbox);
	}
}

////////////////////////////////////////////////
////////////////////////////////////////////////

Ref<EditorInspectorPlugin> EditorInspector::inspector_plugins[MAX_PLUGINS];
int EditorInspector::inspector_plugin_count = 0;

EditorProperty *EditorInspector::instantiate_property_editor(Object *p_object, const Variant::Type p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide) {
	for (int i = inspector_plugin_count - 1; i >= 0; i--) {
		inspector_plugins[i]->parse_property(p_object, p_type, p_path, p_hint, p_hint_text, p_usage, p_wide);
		if (inspector_plugins[i]->added_editors.size()) {
			for (int j = 1; j < inspector_plugins[i]->added_editors.size(); j++) { //only keep first one
				memdelete(inspector_plugins[i]->added_editors[j].property_editor);
			}

			EditorProperty *prop = Object::cast_to<EditorProperty>(inspector_plugins[i]->added_editors[0].property_editor);
			if (prop) {
				inspector_plugins[i]->added_editors.clear();
				return prop;
			} else {
				memdelete(inspector_plugins[i]->added_editors[0].property_editor);
				inspector_plugins[i]->added_editors.clear();
			}
		}
	}
	return nullptr;
}

void EditorInspector::add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
	ERR_FAIL_COND(inspector_plugin_count == MAX_PLUGINS);

	for (int i = 0; i < inspector_plugin_count; i++) {
		if (inspector_plugins[i] == p_plugin) {
			return; //already exists
		}
	}
	inspector_plugins[inspector_plugin_count++] = p_plugin;
}

void EditorInspector::remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin) {
	ERR_FAIL_COND(inspector_plugin_count == MAX_PLUGINS);

	int idx = -1;
	for (int i = 0; i < inspector_plugin_count; i++) {
		if (inspector_plugins[i] == p_plugin) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND_MSG(idx == -1, "Trying to remove nonexistent inspector plugin.");
	for (int i = idx; i < inspector_plugin_count - 1; i++) {
		inspector_plugins[i] = inspector_plugins[i + 1];
	}

	if (idx == inspector_plugin_count - 1) {
		inspector_plugins[idx] = Ref<EditorInspectorPlugin>();
	}

	inspector_plugin_count--;
}

void EditorInspector::cleanup_plugins() {
	for (int i = 0; i < inspector_plugin_count; i++) {
		inspector_plugins[i].unref();
	}
	inspector_plugin_count = 0;
}

void EditorInspector::set_undo_redo(UndoRedo *p_undo_redo) {
	undo_redo = p_undo_redo;
}

String EditorInspector::get_selected_path() const {
	return property_selected;
}

void EditorInspector::_parse_added_editors(VBoxContainer *current_vbox, Ref<EditorInspectorPlugin> ped) {
	for (const EditorInspectorPlugin::AddedEditor &F : ped->added_editors) {
		EditorProperty *ep = Object::cast_to<EditorProperty>(F.property_editor);
		current_vbox->add_child(F.property_editor);

		if (ep) {
			ep->object = object;
			ep->connect("property_changed", callable_mp(this, &EditorInspector::_property_changed));
			ep->connect("property_keyed", callable_mp(this, &EditorInspector::_property_keyed));
			ep->connect("property_deleted", callable_mp(this, &EditorInspector::_property_deleted), varray(), CONNECT_DEFERRED);
			ep->connect("property_keyed_with_value", callable_mp(this, &EditorInspector::_property_keyed_with_value));
			ep->connect("property_checked", callable_mp(this, &EditorInspector::_property_checked));
			ep->connect("selected", callable_mp(this, &EditorInspector::_property_selected));
			ep->connect("multiple_properties_changed", callable_mp(this, &EditorInspector::_multiple_properties_changed));
			ep->connect("resource_selected", callable_mp(this, &EditorInspector::_resource_selected), varray(), CONNECT_DEFERRED);
			ep->connect("object_id_selected", callable_mp(this, &EditorInspector::_object_id_selected), varray(), CONNECT_DEFERRED);

			if (F.properties.size()) {
				if (F.properties.size() == 1) {
					//since it's one, associate:
					ep->property = F.properties[0];
					ep->property_usage = 0;
				}

				if (F.label != String()) {
					ep->set_label(F.label);
				}

				for (int i = 0; i < F.properties.size(); i++) {
					String prop = F.properties[i];

					if (!editor_property_map.has(prop)) {
						editor_property_map[prop] = List<EditorProperty *>();
					}
					editor_property_map[prop].push_back(ep);
				}
			}

			ep->set_read_only(read_only);
			ep->update_property();
			ep->update_reload_status();
			ep->set_deletable(deletable_properties);
			ep->update_cache();
		}
	}
	ped->added_editors.clear();
}

bool EditorInspector::_is_property_disabled_by_feature_profile(const StringName &p_property) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = object->get_class();

	while (class_name != StringName()) {
		if (profile->is_class_property_disabled(class_name, p_property)) {
			return true;
		}
		if (profile->is_class_disabled(class_name)) {
			//won't see properties of a disabled class
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

void EditorInspector::update_tree() {
	//to update properly if all is refreshed
	StringName current_selected = property_selected;
	int current_focusable = -1;

	if (property_focusable != -1) {
		//check focusable is really focusable
		bool restore_focus = false;
		Control *focused = get_focus_owner();
		if (focused) {
			Node *parent = focused->get_parent();
			while (parent) {
				EditorInspector *inspector = Object::cast_to<EditorInspector>(parent);
				if (inspector) {
					restore_focus = inspector == this; //may be owned by another inspector
					break; //exit after the first inspector is found, since there may be nested ones
				}
				parent = parent->get_parent();
			}
		}

		if (restore_focus) {
			current_focusable = property_focusable;
		}
	}

	_clear();

	if (!object) {
		return;
	}

	List<Ref<EditorInspectorPlugin>> valid_plugins;

	for (int i = inspector_plugin_count - 1; i >= 0; i--) { //start by last, so lastly added can override newly added
		if (!inspector_plugins[i]->can_handle(object)) {
			continue;
		}
		valid_plugins.push_back(inspector_plugins[i]);
	}

	bool draw_red = false;

	if (is_inside_tree()) {
		Node *nod = Object::cast_to<Node>(object);
		Node *es = EditorNode::get_singleton()->get_edited_scene();
		if (nod && es != nod && nod->get_owner() != es) {
			draw_red = true;
		}
	}

	//	TreeItem *current_category = nullptr;

	String filter = search_box ? search_box->get_text() : "";
	String group;
	String group_base;
	String subgroup;
	String subgroup_base;
	VBoxContainer *category_vbox = nullptr;

	List<PropertyInfo> plist;
	object->get_property_list(&plist, true);
	_update_script_class_properties(*object, plist);

	HashMap<String, VBoxContainer *> item_path;
	Map<VBoxContainer *, EditorInspectorSection *> section_map;

	item_path[""] = main_vbox;

	Color sscolor = get_theme_color(SNAME("prop_subsection"), SNAME("Editor"));

	for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
		ped->parse_begin(object);
		_parse_added_editors(main_vbox, ped);
	}

	for (List<PropertyInfo>::Element *I = plist.front(); I; I = I->next()) {
		PropertyInfo &p = I->get();

		//make sure the property can be edited

		if (p.usage & PROPERTY_USAGE_SUBGROUP) {
			subgroup = p.name;
			subgroup_base = p.hint_string;

			continue;

		} else if (p.usage & PROPERTY_USAGE_GROUP) {
			group = p.name;
			group_base = p.hint_string;
			subgroup = "";
			subgroup_base = "";

			continue;

		} else if (p.usage & PROPERTY_USAGE_CATEGORY) {
			group = "";
			group_base = "";
			subgroup = "";
			subgroup_base = "";

			if (!show_categories) {
				continue;
			}

			List<PropertyInfo>::Element *N = I->next();
			bool valid = true;
			//if no properties in category, skip
			while (N) {
				if (N->get().usage & PROPERTY_USAGE_EDITOR && (!restrict_to_basic || (N->get().usage & PROPERTY_USAGE_EDITOR_BASIC_SETTING))) {
					break;
				}
				if (N->get().usage & PROPERTY_USAGE_CATEGORY) {
					valid = false;
					break;
				}
				N = N->next();
			}
			if (!valid) {
				continue; //empty, ignore
			}

			EditorInspectorCategory *category = memnew(EditorInspectorCategory);
			main_vbox->add_child(category);
			category_vbox = nullptr; //reset

			String type = p.name;
			if (!ClassDB::class_exists(type) && !ScriptServer::is_global_class(type) && p.hint_string.length() && FileAccess::exists(p.hint_string)) {
				Ref<Script> s = ResourceLoader::load(p.hint_string, "Script");
				String base_type;
				if (s.is_valid()) {
					base_type = s->get_instance_base_type();
				}
				while (s.is_valid()) {
					StringName name = EditorNode::get_editor_data().script_class_get_name(s->get_path());
					String icon_path = EditorNode::get_editor_data().script_class_get_icon_path(name);
					if (name != StringName() && icon_path.length()) {
						category->icon = ResourceLoader::load(icon_path, "Texture");
						break;
					}
					s = s->get_base_script();
				}
				if (category->icon.is_null() && has_theme_icon(base_type, SNAME("EditorIcons"))) {
					category->icon = get_theme_icon(base_type, SNAME("EditorIcons"));
				}
			}
			if (category->icon.is_null()) {
				if (type != String()) { // Can happen for built-in scripts.
					category->icon = EditorNode::get_singleton()->get_class_icon(type, "Object");
				}
			}
			category->label = type;

			if (use_doc_hints) {
				StringName type2 = p.name;
				if (!class_descr_cache.has(type2)) {
					String descr;
					DocTools *dd = EditorHelp::get_doc_data();
					Map<String, DocData::ClassDoc>::Element *E = dd->class_list.find(type2);
					if (E) {
						descr = DTR(E->get().brief_description);
					}
					class_descr_cache[type2] = descr;
				}

				category->set_tooltip(p.name + "::" + (class_descr_cache[type2] == "" ? "" : class_descr_cache[type2]));
			}

			for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
				ped->parse_category(object, p.name);
				_parse_added_editors(main_vbox, ped);
			}

			continue;

		} else if (!(p.usage & PROPERTY_USAGE_EDITOR) || _is_property_disabled_by_feature_profile(p.name) || (restrict_to_basic && !(p.usage & PROPERTY_USAGE_EDITOR_BASIC_SETTING))) {
			continue;
		}

		if (p.name == "script") {
			category_vbox = nullptr; // script should go into its own category
		}

		if (p.usage & PROPERTY_USAGE_HIGH_END_GFX && RS::get_singleton()->is_low_end()) {
			continue; //do not show this property in low end gfx
		}

		if (p.name == "script" && (hide_script || bool(object->call("_hide_script_from_inspector")))) {
			continue;
		}

		String basename = p.name;

		if (subgroup != "") {
			if (subgroup_base != "") {
				if (basename.begins_with(subgroup_base)) {
					basename = basename.replace_first(subgroup_base, "");
				} else if (subgroup_base.begins_with(basename)) {
					//keep it, this is used pretty often
				} else {
					subgroup = ""; //no longer using subgroup base, clear
				}
			}
		}
		if (group != "") {
			if (group_base != "" && subgroup == "") {
				if (basename.begins_with(group_base)) {
					basename = basename.replace_first(group_base, "");
				} else if (group_base.begins_with(basename)) {
					//keep it, this is used pretty often
				} else {
					group = ""; //no longer using group base, clear
					subgroup = "";
				}
			}
		}
		if (subgroup != "") {
			basename = subgroup + "/" + basename;
		}
		if (group != "") {
			basename = group + "/" + basename;
		}

		String name = (basename.find("/") != -1) ? basename.substr(basename.rfind("/") + 1) : basename;

		if (capitalize_paths) {
			int dot = name.find(".");
			if (dot != -1) {
				String ov = name.substr(dot);
				name = name.substr(0, dot);
				name = name.capitalize();
				name += ov;

			} else {
				name = name.capitalize();
			}
		}

		String path;
		{
			int idx = basename.rfind("/");
			if (idx > -1) {
				path = basename.left(idx);
			}
		}

		if (use_filter && filter != "") {
			String cat = path;

			if (capitalize_paths) {
				cat = cat.capitalize();
			}

			if (!filter.is_subsequence_ofi(cat) && !filter.is_subsequence_ofi(name) && property_prefix.to_lower().find(filter.to_lower()) == -1) {
				continue;
			}
		}

		if (category_vbox == nullptr) {
			category_vbox = memnew(VBoxContainer);
			main_vbox->add_child(category_vbox);
		}

		VBoxContainer *current_vbox = main_vbox;

		{
			String acc_path = "";
			int level = 1;
			for (int i = 0; i < path.get_slice_count("/"); i++) {
				String path_name = path.get_slice("/", i);
				if (i > 0) {
					acc_path += "/";
				}
				acc_path += path_name;
				if (!item_path.has(acc_path)) {
					EditorInspectorSection *section = memnew(EditorInspectorSection);
					current_vbox->add_child(section);
					sections.push_back(section);

					if (capitalize_paths) {
						path_name = path_name.capitalize();
					}

					Color c = sscolor;
					c.a /= level;
					section->setup(acc_path, path_name, object, c, use_folding);

					VBoxContainer *vb = section->get_vbox();
					item_path[acc_path] = vb;
					section_map[vb] = section;
				}
				current_vbox = item_path[acc_path];
				level = (MIN(level + 1, 4));
			}

			if (current_vbox == main_vbox) {
				//do not add directly to the main vbox, given it has no spacing
				if (category_vbox == nullptr) {
					category_vbox = memnew(VBoxContainer);
				}
				current_vbox = category_vbox;
			}
		}

		bool checkable = false;
		bool checked = false;
		if (p.usage & PROPERTY_USAGE_CHECKABLE) {
			checkable = true;
			checked = p.usage & PROPERTY_USAGE_CHECKED;
		}

		if (p.usage & PROPERTY_USAGE_RESTART_IF_CHANGED) {
			restart_request_props.insert(p.name);
		}

		String doc_hint;

		if (use_doc_hints) {
			StringName classname = object->get_class_name();
			if (object_class != String()) {
				classname = object_class;
			}
			StringName propname = property_prefix + p.name;
			String descr;
			bool found = false;

			Map<StringName, Map<StringName, String>>::Element *E = descr_cache.find(classname);
			if (E) {
				Map<StringName, String>::Element *F = E->get().find(propname);
				if (F) {
					found = true;
					descr = F->get();
				}
			}

			if (!found) {
				DocTools *dd = EditorHelp::get_doc_data();
				Map<String, DocData::ClassDoc>::Element *F = dd->class_list.find(classname);
				while (F && descr == String()) {
					for (int i = 0; i < F->get().properties.size(); i++) {
						if (F->get().properties[i].name == propname.operator String()) {
							descr = DTR(F->get().properties[i].description);
							break;
						}
					}

					Vector<String> slices = propname.operator String().split("/");
					if (slices.size() == 2 && slices[0].begins_with("theme_override_")) {
						for (int i = 0; i < F->get().theme_properties.size(); i++) {
							if (F->get().theme_properties[i].name == slices[1]) {
								descr = DTR(F->get().theme_properties[i].description);
								break;
							}
						}
					}

					if (!F->get().inherits.is_empty()) {
						F = dd->class_list.find(F->get().inherits);
					} else {
						break;
					}
				}
				descr_cache[classname][propname] = descr;
			}

			doc_hint = descr;
		}

		for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
			bool exclusive = ped->parse_property(object, p.type, p.name, p.hint, p.hint_string, p.usage, wide_editors);

			List<EditorInspectorPlugin::AddedEditor> editors = ped->added_editors; //make a copy, since plugins may be used again in a sub-inspector
			ped->added_editors.clear();

			for (const EditorInspectorPlugin::AddedEditor &F : editors) {
				EditorProperty *ep = Object::cast_to<EditorProperty>(F.property_editor);

				if (ep) {
					//set all this before the control gets the ENTER_TREE notification
					ep->object = object;

					if (F.properties.size()) {
						if (F.properties.size() == 1) {
							//since it's one, associate:
							ep->property = F.properties[0];
							ep->property_usage = p.usage;
							//and set label?
						}

						if (F.label != String()) {
							ep->set_label(F.label);
						} else {
							// Use the existing one.
							ep->set_label(name);
						}
						for (int i = 0; i < F.properties.size(); i++) {
							String prop = F.properties[i];

							if (!editor_property_map.has(prop)) {
								editor_property_map[prop] = List<EditorProperty *>();
							}
							editor_property_map[prop].push_back(ep);
						}
					}
					ep->set_draw_red(draw_red);
					ep->set_use_folding(use_folding);
					ep->set_checkable(checkable);
					ep->set_checked(checked);
					ep->set_keying(keying);

					ep->set_read_only(read_only);
					ep->set_deletable(deletable_properties);
				}

				current_vbox->add_child(F.property_editor);

				if (ep) {
					ep->connect("property_changed", callable_mp(this, &EditorInspector::_property_changed));
					if (p.usage & PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED) {
						ep->connect("property_changed", callable_mp(this, &EditorInspector::_property_changed_update_all), varray(), CONNECT_DEFERRED);
					}
					ep->connect("property_keyed", callable_mp(this, &EditorInspector::_property_keyed));
					ep->connect("property_deleted", callable_mp(this, &EditorInspector::_property_deleted), varray(), CONNECT_DEFERRED);
					ep->connect("property_keyed_with_value", callable_mp(this, &EditorInspector::_property_keyed_with_value));
					ep->connect("property_checked", callable_mp(this, &EditorInspector::_property_checked));
					ep->connect("selected", callable_mp(this, &EditorInspector::_property_selected));
					ep->connect("multiple_properties_changed", callable_mp(this, &EditorInspector::_multiple_properties_changed));
					ep->connect("resource_selected", callable_mp(this, &EditorInspector::_resource_selected), varray(), CONNECT_DEFERRED);
					ep->connect("object_id_selected", callable_mp(this, &EditorInspector::_object_id_selected), varray(), CONNECT_DEFERRED);
					if (doc_hint != String()) {
						ep->set_tooltip(property_prefix + p.name + "::" + doc_hint);
					} else {
						ep->set_tooltip(property_prefix + p.name);
					}
					ep->update_property();
					ep->update_reload_status();
					ep->update_cache();

					if (current_selected && ep->property == current_selected) {
						ep->select(current_focusable);
					}
				}
			}

			if (exclusive) {
				break;
			}
		}
	}

	for (Ref<EditorInspectorPlugin> &ped : valid_plugins) {
		ped->parse_end();
		_parse_added_editors(main_vbox, ped);
	}

	//see if this property exists and should be kept
}

void EditorInspector::update_property(const String &p_prop) {
	if (!editor_property_map.has(p_prop)) {
		return;
	}

	for (EditorProperty *E : editor_property_map[p_prop]) {
		E->update_property();
		E->update_reload_status();
		E->update_cache();
	}
}

void EditorInspector::_clear() {
	while (main_vbox->get_child_count()) {
		memdelete(main_vbox->get_child(0));
	}
	property_selected = StringName();
	property_focusable = -1;
	editor_property_map.clear();
	sections.clear();
	pending.clear();
	restart_request_props.clear();
}

Object *EditorInspector::get_edited_object() {
	return object;
}

void EditorInspector::edit(Object *p_object) {
	if (object == p_object) {
		return;
	}
	if (object) {
		_clear();
		object->disconnect("property_list_changed", callable_mp(this, &EditorInspector::_changed_callback));
	}

	object = p_object;

	if (object) {
		update_scroll_request = 0; //reset
		if (scroll_cache.has(object->get_instance_id())) { //if exists, set something else
			update_scroll_request = scroll_cache[object->get_instance_id()]; //done this way because wait until full size is accommodated
		}
		object->connect("property_list_changed", callable_mp(this, &EditorInspector::_changed_callback));
		update_tree();
	}
}

void EditorInspector::set_keying(bool p_active) {
	if (keying == p_active) {
		return;
	}
	keying = p_active;
	update_tree();
}

void EditorInspector::set_read_only(bool p_read_only) {
	read_only = p_read_only;
	update_tree();
}

bool EditorInspector::is_capitalize_paths_enabled() const {
	return capitalize_paths;
}

void EditorInspector::set_enable_capitalize_paths(bool p_capitalize) {
	capitalize_paths = p_capitalize;
	update_tree();
}

void EditorInspector::set_autoclear(bool p_enable) {
	autoclear = p_enable;
}

void EditorInspector::set_show_categories(bool p_show) {
	show_categories = p_show;
	update_tree();
}

void EditorInspector::set_use_doc_hints(bool p_enable) {
	use_doc_hints = p_enable;
	update_tree();
}

void EditorInspector::set_hide_script(bool p_hide) {
	hide_script = p_hide;
	update_tree();
}

void EditorInspector::set_use_filter(bool p_use) {
	use_filter = p_use;
	update_tree();
}

void EditorInspector::register_text_enter(Node *p_line_edit) {
	search_box = Object::cast_to<LineEdit>(p_line_edit);
	if (search_box) {
		search_box->connect("text_changed", callable_mp(this, &EditorInspector::_filter_changed));
	}
}

void EditorInspector::_filter_changed(const String &p_text) {
	_clear();
	update_tree();
}

void EditorInspector::set_use_folding(bool p_enable) {
	use_folding = p_enable;
	update_tree();
}

bool EditorInspector::is_using_folding() {
	return use_folding;
}

void EditorInspector::collapse_all_folding() {
	for (EditorInspectorSection *E : sections) {
		E->fold();
	}

	for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
		for (EditorProperty *E : F->get()) {
			E->collapse_all_folding();
		}
	}
}

void EditorInspector::expand_all_folding() {
	for (EditorInspectorSection *E : sections) {
		E->unfold();
	}
	for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
		for (EditorProperty *E : F->get()) {
			E->expand_all_folding();
		}
	}
}

void EditorInspector::set_scroll_offset(int p_offset) {
	set_v_scroll(p_offset);
}

int EditorInspector::get_scroll_offset() const {
	return get_v_scroll();
}

void EditorInspector::set_use_wide_editors(bool p_enable) {
	wide_editors = p_enable;
}

void EditorInspector::_update_inspector_bg() {
	if (sub_inspector) {
		int count_subinspectors = 0;
		Node *n = get_parent();
		while (n) {
			EditorInspector *ei = Object::cast_to<EditorInspector>(n);
			if (ei && ei->sub_inspector) {
				count_subinspectors++;
			}
			n = n->get_parent();
		}
		count_subinspectors = MIN(15, count_subinspectors);
		add_theme_style_override("bg", get_theme_stylebox("sub_inspector_bg" + itos(count_subinspectors), "Editor"));
	} else {
		add_theme_style_override("bg", get_theme_stylebox(SNAME("bg"), SNAME("Tree")));
	}
}
void EditorInspector::set_sub_inspector(bool p_enable) {
	sub_inspector = p_enable;
	if (!is_inside_tree()) {
		return;
	}

	_update_inspector_bg();
}

void EditorInspector::set_use_deletable_properties(bool p_enabled) {
	deletable_properties = p_enabled;
}

void EditorInspector::_edit_request_change(Object *p_object, const String &p_property) {
	if (object != p_object) { //may be undoing/redoing for a non edited object, so ignore
		return;
	}

	if (changing) {
		return;
	}

	if (p_property == String()) {
		update_tree_pending = true;
	} else {
		pending.insert(p_property);
	}
}

void EditorInspector::_edit_set(const String &p_name, const Variant &p_value, bool p_refresh_all, const String &p_changed_field) {
	if (autoclear && editor_property_map.has(p_name)) {
		for (EditorProperty *E : editor_property_map[p_name]) {
			if (E->is_checkable()) {
				E->set_checked(true);
			}
		}
	}

	if (!undo_redo || bool(object->call("_dont_undo_redo"))) {
		object->set(p_name, p_value);
		if (p_refresh_all) {
			_edit_request_change(object, "");
		} else {
			_edit_request_change(object, p_name);
		}

		emit_signal(_prop_edited, p_name);

	} else if (Object::cast_to<MultiNodeEdit>(object)) {
		Object::cast_to<MultiNodeEdit>(object)->set_property_field(p_name, p_value, p_changed_field);
		_edit_request_change(object, p_name);
		emit_signal(_prop_edited, p_name);
	} else {
		undo_redo->create_action(vformat(TTR("Set %s"), p_name), UndoRedo::MERGE_ENDS);
		undo_redo->add_do_property(object, p_name, p_value);
		undo_redo->add_undo_property(object, p_name, object->get(p_name));

		Variant v_undo_redo = (Object *)undo_redo;
		Variant v_object = object;
		Variant v_name = p_name;
		for (int i = 0; i < EditorNode::get_singleton()->get_editor_data().get_undo_redo_inspector_hook_callback().size(); i++) {
			const Callable &callback = EditorNode::get_singleton()->get_editor_data().get_undo_redo_inspector_hook_callback()[i];

			const Variant *p_arguments[] = { &v_undo_redo, &v_object, &v_name, &p_value };
			Variant return_value;
			Callable::CallError call_error;

			callback.call(p_arguments, 4, return_value, call_error);
			if (call_error.error != Callable::CallError::CALL_OK) {
				ERR_PRINT("Invalid UndoRedo callback.");
			}
		}

		if (p_refresh_all) {
			undo_redo->add_do_method(this, "_edit_request_change", object, "");
			undo_redo->add_undo_method(this, "_edit_request_change", object, "");
		} else {
			undo_redo->add_do_method(this, "_edit_request_change", object, p_name);
			undo_redo->add_undo_method(this, "_edit_request_change", object, p_name);
		}

		Resource *r = Object::cast_to<Resource>(object);
		if (r) {
			if (String(p_name) == "resource_local_to_scene") {
				bool prev = object->get(p_name);
				bool next = p_value;
				if (next) {
					undo_redo->add_do_method(r, "setup_local_to_scene");
				}
				if (prev) {
					undo_redo->add_undo_method(r, "setup_local_to_scene");
				}
			}
		}
		undo_redo->add_do_method(this, "emit_signal", _prop_edited, p_name);
		undo_redo->add_undo_method(this, "emit_signal", _prop_edited, p_name);
		undo_redo->commit_action();
	}

	if (editor_property_map.has(p_name)) {
		for (EditorProperty *E : editor_property_map[p_name]) {
			E->update_reload_status();
		}
	}
}

void EditorInspector::_property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing) {
	// The "changing" variable must be true for properties that trigger events as typing occurs,
	// like "text_changed" signal. E.g. text property of Label, Button, RichTextLabel, etc.
	if (p_changing) {
		this->changing++;
	}

	_edit_set(p_path, p_value, false, p_name);

	if (p_changing) {
		this->changing--;
	}

	if (restart_request_props.has(p_path)) {
		emit_signal(SNAME("restart_requested"));
	}
}

void EditorInspector::_property_changed_update_all(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing) {
	update_tree();
}

void EditorInspector::_multiple_properties_changed(Vector<String> p_paths, Array p_values) {
	ERR_FAIL_COND(p_paths.size() == 0 || p_values.size() == 0);
	ERR_FAIL_COND(p_paths.size() != p_values.size());
	String names;
	for (int i = 0; i < p_paths.size(); i++) {
		if (i > 0) {
			names += ",";
		}
		names += p_paths[i];
	}
	undo_redo->create_action(TTR("Set Multiple:") + " " + names, UndoRedo::MERGE_ENDS);
	for (int i = 0; i < p_paths.size(); i++) {
		_edit_set(p_paths[i], p_values[i], false, "");
		if (restart_request_props.has(p_paths[i])) {
			emit_signal(SNAME("restart_requested"));
		}
	}
	changing++;
	undo_redo->commit_action();
	changing--;
}

void EditorInspector::_property_keyed(const String &p_path, bool p_advance) {
	if (!object) {
		return;
	}

	emit_signal(SNAME("property_keyed"), p_path, object->get(p_path), p_advance); //second param is deprecated
}

void EditorInspector::_property_deleted(const String &p_path) {
	if (!object) {
		return;
	}

	emit_signal(SNAME("property_deleted"), p_path); //second param is deprecated
}

void EditorInspector::_property_keyed_with_value(const String &p_path, const Variant &p_value, bool p_advance) {
	if (!object) {
		return;
	}

	emit_signal(SNAME("property_keyed"), p_path, p_value, p_advance); //second param is deprecated
}

void EditorInspector::_property_checked(const String &p_path, bool p_checked) {
	if (!object) {
		return;
	}

	//property checked
	if (autoclear) {
		if (!p_checked) {
			object->set(p_path, Variant());
		} else {
			Variant to_create;
			List<PropertyInfo> pinfo;
			object->get_property_list(&pinfo);
			for (const PropertyInfo &E : pinfo) {
				if (E.name == p_path) {
					Callable::CallError ce;
					Variant::construct(E.type, to_create, nullptr, 0, ce);
					break;
				}
			}
			object->set(p_path, to_create);
		}

		if (editor_property_map.has(p_path)) {
			for (EditorProperty *E : editor_property_map[p_path]) {
				E->update_property();
				E->update_reload_status();
				E->update_cache();
			}
		}

	} else {
		emit_signal(SNAME("property_toggled"), p_path, p_checked);
	}
}

void EditorInspector::_property_selected(const String &p_path, int p_focusable) {
	property_selected = p_path;
	property_focusable = p_focusable;
	//deselect the others
	for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
		if (F->key() == property_selected) {
			continue;
		}
		for (EditorProperty *E : F->get()) {
			if (E->is_selected()) {
				E->deselect();
			}
		}
	}

	emit_signal(SNAME("property_selected"), p_path);
}

void EditorInspector::_object_id_selected(const String &p_path, ObjectID p_id) {
	emit_signal(SNAME("object_id_selected"), p_id);
}

void EditorInspector::_resource_selected(const String &p_path, RES p_resource) {
	emit_signal(SNAME("resource_selected"), p_resource, p_path);
}

void EditorInspector::_node_removed(Node *p_node) {
	if (p_node == object) {
		edit(nullptr);
	}
}

void EditorInspector::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", callable_mp(this, &EditorInspector::_feature_profile_changed));
		set_process(is_visible_in_tree());
		_update_inspector_bg();
	}

	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (!sub_inspector) {
			get_tree()->connect("node_removed", callable_mp(this, &EditorInspector::_node_removed));
		}
	}
	if (p_what == NOTIFICATION_PREDELETE) {
		edit(nullptr); //just in case
	}
	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (!sub_inspector) {
			get_tree()->disconnect("node_removed", callable_mp(this, &EditorInspector::_node_removed));
		}
		edit(nullptr);
	}

	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		set_process(is_visible_in_tree());
	}

	if (p_what == NOTIFICATION_PROCESS) {
		if (update_scroll_request >= 0) {
			get_v_scrollbar()->call_deferred(SNAME("set_value"), update_scroll_request);
			update_scroll_request = -1;
		}
		if (refresh_countdown > 0) {
			refresh_countdown -= get_process_delta_time();
			if (refresh_countdown <= 0) {
				for (Map<StringName, List<EditorProperty *>>::Element *F = editor_property_map.front(); F; F = F->next()) {
					for (EditorProperty *E : F->get()) {
						if (!E->is_cache_valid()) {
							E->update_property();
							E->update_reload_status();
							E->update_cache();
						}
					}
				}
				refresh_countdown = float(EditorSettings::get_singleton()->get("docks/property_editor/auto_refresh_interval"));
			}
		}

		changing++;

		if (update_tree_pending) {
			update_tree();
			update_tree_pending = false;
			pending.clear();

		} else {
			while (pending.size()) {
				StringName prop = pending.front()->get();
				if (editor_property_map.has(prop)) {
					for (EditorProperty *E : editor_property_map[prop]) {
						E->update_property();
						E->update_reload_status();
						E->update_cache();
					}
				}
				pending.erase(pending.front());
			}
		}

		changing--;
	}

	if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {
		_update_inspector_bg();

		update_tree();
	}
}

void EditorInspector::_changed_callback() {
	//this is called when property change is notified via notify_property_list_changed()
	if (object != nullptr) {
		_edit_request_change(object, String());
	}
}

void EditorInspector::_vscroll_changed(double p_offset) {
	if (update_scroll_request >= 0) { //waiting, do nothing
		return;
	}

	if (object) {
		scroll_cache[object->get_instance_id()] = p_offset;
	}
}

void EditorInspector::set_property_prefix(const String &p_prefix) {
	property_prefix = p_prefix;
}

String EditorInspector::get_property_prefix() const {
	return property_prefix;
}

void EditorInspector::set_object_class(const String &p_class) {
	object_class = p_class;
}

String EditorInspector::get_object_class() const {
	return object_class;
}

void EditorInspector::_feature_profile_changed() {
	update_tree();
}

void EditorInspector::_update_script_class_properties(const Object &p_object, List<PropertyInfo> &r_list) const {
	Ref<Script> script = p_object.get_script();
	if (script.is_null()) {
		return;
	}

	List<Ref<Script>> classes;

	// NodeC -> NodeB -> NodeA
	while (script.is_valid()) {
		classes.push_front(script);
		script = script->get_base_script();
	}

	if (classes.is_empty()) {
		return;
	}

	// Script Variables -> to insert: NodeC..B..A -> bottom (insert_here)
	List<PropertyInfo>::Element *script_variables = nullptr;
	List<PropertyInfo>::Element *bottom = nullptr;
	List<PropertyInfo>::Element *insert_here = nullptr;
	for (List<PropertyInfo>::Element *E = r_list.front(); E; E = E->next()) {
		PropertyInfo &pi = E->get();
		if (pi.name != "Script Variables") {
			continue;
		}
		script_variables = E;
		bottom = r_list.insert_after(script_variables, PropertyInfo());
		insert_here = bottom;
		break;
	}

	Set<StringName> added;
	for (const Ref<Script> &s : classes) {
		String path = s->get_path();
		String name = EditorNode::get_editor_data().script_class_get_name(path);
		if (name.is_empty()) {
			if (!path.is_empty() && path.find("::") == -1) {
				name = path.get_file();
			} else {
				name = TTR("Built-in script");
			}
		}

		List<PropertyInfo> props;
		s->get_script_property_list(&props);

		// Script Variables -> NodeA -> bottom (insert_here)
		List<PropertyInfo>::Element *category = r_list.insert_before(insert_here, PropertyInfo(Variant::NIL, name, PROPERTY_HINT_NONE, path, PROPERTY_USAGE_CATEGORY));

		// Script Variables -> NodeA -> A props... -> bottom (insert_here)
		for (List<PropertyInfo>::Element *P = props.front(); P; P = P->next()) {
			PropertyInfo &pi = P->get();
			if (added.has(pi.name)) {
				continue;
			}
			added.insert(pi.name);

			r_list.insert_before(insert_here, pi);
		}

		// Script Variables -> NodeA (insert_here) -> A props... -> bottom
		insert_here = category;
	}

	// NodeC -> C props... -> NodeB..C..
	r_list.erase(script_variables);
	List<PropertyInfo>::Element *to_delete = bottom->next();
	while (to_delete && !(to_delete->get().usage & PROPERTY_USAGE_CATEGORY)) {
		r_list.erase(to_delete);
		to_delete = bottom->next();
	}
	r_list.erase(bottom);
}

void EditorInspector::set_restrict_to_basic_settings(bool p_restrict) {
	restrict_to_basic = p_restrict;
	update_tree();
}

void EditorInspector::_bind_methods() {
	ClassDB::bind_method("_edit_request_change", &EditorInspector::_edit_request_change);

	ADD_SIGNAL(MethodInfo("property_selected", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_keyed", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_deleted", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::OBJECT, "res"), PropertyInfo(Variant::STRING, "prop")));
	ADD_SIGNAL(MethodInfo("object_id_selected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("property_edited", PropertyInfo(Variant::STRING, "property")));
	ADD_SIGNAL(MethodInfo("property_toggled", PropertyInfo(Variant::STRING, "property"), PropertyInfo(Variant::BOOL, "checked")));
	ADD_SIGNAL(MethodInfo("restart_requested"));
}

EditorInspector::EditorInspector() {
	object = nullptr;
	undo_redo = nullptr;
	main_vbox = memnew(VBoxContainer);
	main_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_theme_constant_override("separation", 0);
	add_child(main_vbox);
	set_enable_h_scroll(false);
	set_enable_v_scroll(true);

	wide_editors = false;
	show_categories = false;
	hide_script = true;
	use_doc_hints = false;
	capitalize_paths = true;
	use_filter = false;
	autoclear = false;
	changing = 0;
	use_folding = false;
	update_all_pending = false;
	update_tree_pending = false;
	read_only = false;
	search_box = nullptr;
	keying = false;
	_prop_edited = "property_edited";
	set_process(false);
	property_focusable = -1;
	sub_inspector = false;
	deletable_properties = false;

	get_v_scrollbar()->connect("value_changed", callable_mp(this, &EditorInspector::_vscroll_changed));
	update_scroll_request = -1;
	if (EditorSettings::get_singleton()) {
		refresh_countdown = float(EditorSettings::get_singleton()->get("docks/property_editor/auto_refresh_interval"));
	} else {
		//used when class is created by the docgen to dump default values of everything bindable, editorsettings may not be created
		refresh_countdown = 0.33;
	}
}
