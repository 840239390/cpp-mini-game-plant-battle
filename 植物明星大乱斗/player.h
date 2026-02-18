#pragma once
#include "camera.h"
#include "animation.h"
#include "vector2.h"
#include "graphics.h"
#include <iostream>
#include "platform.h"
#include "player_id.h"
#include "bullet.h"
#include "particle.h"
using namespace std;

extern IMAGE img_1P_cursor;
extern IMAGE img_2P_cursor;

extern bool is_debug;
extern vector<Bullet*> bullet_list;
extern vector<Platform> platform_list;
extern Atlas atlas_run_effect;
extern Atlas atlas_jump_effect;
extern Atlas atlas_run_effect;

class Player
{
public:
	Player(bool facing_right = true) : is_facing_right(facing_right)
	{
		current_animation = is_facing_right ? &animation_idle_right : &animation_idle_left;

		timer_attack_cd.set_wait_time(attack_cd);
		timer_attack_cd.set_one_shot(true);
		timer_attack_cd.set_callback(
			[&]()
			{
				can_attack = true;
			});

		timer_invulnerable.set_wait_time(750);
		timer_invulnerable.set_one_shot(true);
		timer_invulnerable.set_callback(
			[&]()
			{
				is_invulnerable = false;
			});

		timer_invulnerable_blink.set_wait_time(100);
		timer_invulnerable_blink.set_callback(
			[&]()
			{
				is_showing_sketch_frame = !is_showing_sketch_frame;
			});

		timer_run_effect_generation.set_wait_time(75);
		timer_run_effect_generation.set_callback(
			[&]()
			{
				Vector2 particle_position;
				IMAGE* frame = atlas_run_effect.get_image(0);
				particle_position.x = position.x + (size.x - frame->getwidth()) / 2;
				particle_position.y = position.y + size.y - frame->getheight();
				particle_list.emplace_back(particle_position, &atlas_run_effect, 45);
			});

		timer_die_effect_generation.set_wait_time(35);
		timer_die_effect_generation.set_callback(
			[&]()
			{
				Vector2 particle_position;
				IMAGE* frame = atlas_run_effect.get_image(0);
				particle_position.x = position.x + (size.x - frame->getwidth()) / 2;
				particle_position.y = position.y + size.y - frame->getheight();
				particle_list.emplace_back(particle_position, &atlas_run_effect, 150);
			});

		animation_jump_effect.set_atlas(&atlas_jump_effect);
		animation_jump_effect.set_interval(25);
		animation_jump_effect.set_loop(false);
		animation_jump_effect.set_callback(
			[&]()
			{
				is_jump_effect_visible = false;
			});

		animation_land_effect.set_atlas(&atlas_jump_effect);
		animation_land_effect.set_interval(25);
		animation_land_effect.set_loop(false);
		animation_land_effect.set_callback(
			[&]()
			{
				is_land_effect_visible = false;
			});
	}
	~Player() = default;

	virtual void on_update(int delta)
	{
		int direction = is_right_key_down - is_left_key_down;

		if (direction != 0)
		{
			is_facing_right = direction > 0;
			current_animation = is_facing_right ? &animation_run_right : &animation_run_left;
			float distance = direction * run_velocity * delta;
			on_run(distance);
		}
		else
		{
			current_animation = is_facing_right ? &animation_idle_right : &animation_idle_left;
			timer_run_effect_generation.pause();
		}

		if (is_attacking_ex)
			current_animation = is_facing_right ? &animation_attack_ex_right : &animation_attack_ex_left;

		if (hp <= 0)
		{
			current_animation = last_hurt_direction.x < 0 ? &animation_die_left : &animation_die_right;
			timer_die_effect_generation.on_update(delta);
		}
		current_animation->on_update(delta);

		animation_jump_effect.on_update(delta);
		animation_land_effect.on_update(delta);

		timer_attack_cd.on_update(delta);
		timer_invulnerable.on_update(delta);
		timer_invulnerable_blink.on_update(delta);
		timer_run_effect_generation.on_update(delta);
			

		particle_list.erase(remove_if(particle_list.begin(), particle_list.end(),
			[](const Particle& particle)
			{
				return !particle.check_valid();
			}),
			particle_list.end());

		for (Particle& particle : particle_list)
			particle.on_update(delta);

		if (is_showing_sketch_frame)
			sketch_image(current_animation->get_frame(), &img_sketch);
		move_and_collide(delta);
	}

	virtual void on_draw(const Camera& camera)
	{
		if (is_jump_effect_visible)
			animation_jump_effect.on_draw(camera, (int)position_jump_effect.x, (int)position_jump_effect.y);

		if (is_land_effect_visible)
			animation_land_effect.on_draw(camera, (int)position_land_effect.x, (int)position_land_effect.y);

		for (Particle& particle : particle_list)
			particle.on_draw(camera);

		if (hp > 0 && is_invulnerable && is_showing_sketch_frame)
			putimage_alpha(camera, (int)position.x, (int)position.y, &img_sketch);
		else
			current_animation->on_draw(camera, (int)position.x, (int)position.y);

		if (is_debug)
		{
			setlinecolor(RGB(0,125,125));
			rectangle((int)position.x, (int)position.y, (int)(position.x + size.x), (int)(position.y + size.y));
		}
	}

	virtual void on_input(const ExMessage& msg)
	{
		switch (msg.message)
		{
		case WM_KEYDOWN:
			switch (id)
			{
			case PlayerID::P1:
				switch (msg.vkcode)
				{
				case 0x41:	//'A'
					is_left_key_down = true;
					break;
				case 0x44:	//'D'
					is_right_key_down = true;
					break;
				case 0x57:	//'w'
					on_jump();
					break;
				case 0x46:	//'F'
					if (can_attack)
					{
						on_attack();
						can_attack = false;
						timer_attack_cd.restart();
					}
					break;
				case 0x47:	//'G'
					if (mp >= 100)
					{
						on_attack_ex();
						mp = 0;
					}
					break;
				}
				break;
			case PlayerID::P2:
				switch (msg.vkcode)
				{
				case VK_LEFT:	//'←'
					is_left_key_down = true;
					break;
				case VK_RIGHT:	//'→'
					is_right_key_down = true;
					break;
				case VK_UP:	//‘上’
					on_jump();
					break;
				case VK_OEM_PERIOD:
					if (can_attack)
					{
						on_attack();
						can_attack = false;
						timer_attack_cd.restart();
					}
					break;
				case VK_OEM_2:	//'G'
					if (mp >= 100)
					{
						on_attack_ex();
						mp = 0;
					}
					break;
				}
				break;
			}
			break;
		case WM_KEYUP:
			switch (id)
			{
			case PlayerID::P1:
				switch (msg.vkcode)
				{
				case 0x41:	//'A'
					is_left_key_down = false;
					break;
				case 0x44:	//'D'
					is_right_key_down = false;
					break;
				}
				break;
			case PlayerID::P2:
				switch (msg.vkcode)
				{
				case VK_LEFT:	//'←'
					is_left_key_down = false;
					break;
				case VK_RIGHT:	//'→'
					is_right_key_down = false;
					break;
				}
				break;
			}
			break;
		}
	}

	void set_id(PlayerID id)
	{
		this->id = id;
	}

	void set_position(float x, float y)
	{
		this->position.x = x, this->position.y = y;
	}

	void on_run(float distance)
	{
		if (is_attacking_ex)
			return;
		position.x += distance;

		timer_run_effect_generation.resume();
	}

	virtual void on_jump()
	{
		if (velocity.y != 0 || is_attacking_ex)
			return;
		velocity.y += jump_velocity;
		is_jump_effect_visible = true;
		animation_jump_effect.reset();

		IMAGE* effect_frame = animation_jump_effect.get_frame();
		position_jump_effect.x = position.x + (size.x - effect_frame->getwidth()) / 2;
		position_jump_effect.y = position.y + size.y - effect_frame->getheight();
	}
	
	virtual void on_land()
	{
		is_land_effect_visible = true;
		animation_land_effect.reset();

		IMAGE* effect_frame = animation_land_effect.get_frame();
		position_land_effect.x = position.x + (size.x - effect_frame->getwidth()) / 2;
		position_land_effect.y = position.y + size.y - effect_frame->getheight();
	}

	const Vector2& get_position() const
	{
		return position;
	}

	const Vector2& get_size() const
	{
		return size;
	}
	virtual void on_attack() {}
	virtual void on_attack_ex() {}

	int get_hp()
	{
		return hp;
	}

	int get_mp()
	{
		return mp;
	}

	void set_hp(int val)
	{
		hp = val;
	}

	void make_invulnerable()
	{
		is_invulnerable = true;
		timer_invulnerable.restart();
	}
protected:
	void move_and_collide(int delta)
	{
		float last_velocity_y = velocity.y;

		velocity.y += gravity * delta;
		position += velocity * (float)delta;

		if (hp <= 0)
			return;

		if (velocity.y > 0)
		{
			for (const Platform& platform : platform_list)
			{
				const Platform::CollisionShape& shape = platform.shape;
				bool is_collide_x = (max(position.x + size.x, shape.right)
					- min(position.x, shape.left) <= size.x + (shape.right - shape.left));
				bool is_collide_y = (position.y + size.y >= shape.y) && 
					(position.y + size.y - velocity.y * delta <= shape.y);

				if (is_collide_x && is_collide_y)
				{
					float delta_pos_y = velocity.y * delta;
					float last_tick_foot_pos_y = position.y + size.y - delta_pos_y;
					if(last_tick_foot_pos_y <= shape.y)
					position.y = shape.y - size.y;
					velocity.y = 0;

					if (last_velocity_y != 0)
						on_land();
					break;
				}
			}
		}

		if (!is_invulnerable)
		{
			for (Bullet* bullet : bullet_list)
			{
				if (!bullet->get_valid() || bullet->get_collide_target() != id)
					continue;

				if (bullet->check_collide(position, size))
				{
					make_invulnerable();
					bullet->on_collide();
					bullet->set_valid(false);
					hp -= bullet->get_damage();
					last_hurt_direction = bullet->get_position() - position;
					if (hp <= 0)
					{
						velocity.x = last_hurt_direction.x < 0 ? 0.35f : -0.35f;
						velocity.y = -1.0f;
					}
				}
			}
		}
	}
protected:
	const float jump_velocity = -0.85f;
	const float gravity = 1.6e-3f;
	const float run_velocity = 0.55;	//玩家速度
protected:
	Vector2 position;
	Vector2 velocity; //角色速度
	Vector2 size;		//角色尺寸

	Animation animation_idle_left;
	Animation animation_idle_right;
	Animation animation_run_left;
	Animation animation_run_right;
	Animation animation_attack_ex_left;
	Animation animation_attack_ex_right;
	Animation animation_jump_effect;	//跳跃特效动画
	Animation animation_land_effect;	//落地特效动画
	Animation animation_die_right;
	Animation animation_die_left;

	bool is_jump_effect_visible = false;
	bool is_land_effect_visible = false;

	Vector2 position_jump_effect;
	Vector2 position_land_effect;
	Animation* current_animation = nullptr;

	PlayerID id = PlayerID::P1;

	bool is_left_key_down = false;
	bool is_right_key_down = false;

	bool is_facing_right = true;

	int attack_cd = 500;
	bool can_attack = true;
	Timer timer_attack_cd;

	int mp = 0;
	int hp = 100;

	bool is_attacking_ex = false;


	IMAGE img_sketch;			//动画帧剪影
	bool is_invulnerable = false;			//是否处于无敌
	bool is_showing_sketch_frame = false;	//当前帧是否应该显示剪影
	Timer timer_invulnerable;			// 无敌状态定时器
	Timer timer_invulnerable_blink;		//无敌状态闪烁定时器

	vector<Particle> particle_list;

	Timer timer_run_effect_generation;	//跑动特效粒子定时器
	Timer timer_die_effect_generation;	//死亡特效粒子定时器

	Vector2 last_hurt_direction;
};