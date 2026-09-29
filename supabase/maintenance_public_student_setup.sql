-- Maintenance Management System - complete shared Supabase schema
-- Run this file in Supabase SQL Editor after the base schema.
-- This file is designed to be safe to run more than once.

alter table public.complaints alter column student_id drop not null;
alter table public.complaints add column if not exists registration_no text;
alter table public.complaints add column if not exists student_name text;
alter table public.complaints add column if not exists repair_items jsonb default '[]'::jsonb;
alter table public.complaints add column if not exists repair_cost numeric(10,2) default 0;
alter table public.complaints add column if not exists completed_at timestamptz;
alter table public.complaints add column if not exists completed_by uuid references public.profiles(id);

-- Rooms and cupboards are intentionally separate so you can provide the actual
-- room/cupboard list later without changing the application structure.
create table if not exists public.rooms (
  id uuid primary key default gen_random_uuid(),
  room_code text not null unique,
  room_name text,
  description text,
  active boolean not null default true,
  created_at timestamptz not null default now()
);

create table if not exists public.cupboards (
  id uuid primary key default gen_random_uuid(),
  cupboard_code text not null unique,
  cupboard_name text,
  room_id uuid references public.rooms(id) on delete set null,
  description text,
  active boolean not null default true,
  created_at timestamptz not null default now()
);

-- Allows one item to exist in several rooms/cupboards.
create table if not exists public.stock_locations (
  id uuid primary key default gen_random_uuid(),
  stock_id uuid not null references public.stock(id) on delete cascade,
  room_id uuid references public.rooms(id) on delete set null,
  cupboard_id uuid references public.cupboards(id) on delete set null,
  quantity integer not null default 0 check (quantity >= 0),
  updated_at timestamptz not null default now(),
  unique(stock_id, room_id, cupboard_id)
);

-- Every change to inventory is recorded. The stock table remains the fast
-- current-total view; this table is the permanent history.
create table if not exists public.inventory_transactions (
  id uuid primary key default gen_random_uuid(),
  stock_id uuid not null references public.stock(id) on delete restrict,
  item_name text not null,
  transaction_type text not null check (
    transaction_type in (
      'STOCK_ADDED',
      'STOCK_ISSUED',
      'STOCK_RETURNED',
      'USED_IN_REPAIR',
      'STOCK_ADJUSTMENT',
      'TRANSFER_IN',
      'TRANSFER_OUT'
    )
  ),
  quantity integer not null check (quantity > 0),
  from_room_id uuid references public.rooms(id) on delete set null,
  from_cupboard_id uuid references public.cupboards(id) on delete set null,
  to_room_id uuid references public.rooms(id) on delete set null,
  to_cupboard_id uuid references public.cupboards(id) on delete set null,
  related_complaint_id bigint references public.complaints(complaint_no) on delete set null,
  performed_by uuid references public.profiles(id) on delete set null,
  performed_by_name text,
  issued_to text,
  purpose text,
  notes text,
  created_at timestamptz not null default now()
);

-- Records an item being taken/given and later returned.
create table if not exists public.item_issues (
  id uuid primary key default gen_random_uuid(),
  stock_id uuid not null references public.stock(id) on delete restrict,
  item_name text not null,
  quantity_issued integer not null check (quantity_issued > 0),
  quantity_returned integer not null default 0 check (quantity_returned >= 0),
  issued_to text not null,
  issued_by uuid references public.profiles(id) on delete set null,
  issued_by_name text,
  issue_room_id uuid references public.rooms(id) on delete set null,
  issue_cupboard_id uuid references public.cupboards(id) on delete set null,
  purpose text,
  issue_date timestamptz not null default now(),
  expected_return_date date,
  return_date timestamptz,
  status text not null default 'ISSUED' check (
    status in ('ISSUED','PARTIALLY_RETURNED','RETURNED','CANCELLED')
  ),
  notes text
);

create table if not exists public.duties (
  id uuid primary key default gen_random_uuid(),
  duty_date date not null,
  duty_day text not null,
  member_1 text,
  member_2 text,
  member_3 text,
  notes text,
  created_at timestamptz not null default now(),
  unique(duty_date)
);

create table if not exists public.srd_members (
  id uuid primary key references public.profiles(id) on delete cascade,
  display_name text not null,
  active boolean not null default true,
  created_at timestamptz not null default now()
);

alter table public.srd_requests add column if not exists purpose text;
alter table public.srd_requests add column if not exists approved_by uuid references public.profiles(id);
alter table public.srd_requests add column if not exists approved_at timestamptz;
alter table public.srd_requests add column if not exists issued_quantity integer default 0;
alter table public.srd_requests add column if not exists fulfilled_at timestamptz;

-- Public student complaint entry. Students do not need Supabase accounts.
create or replace function public.submit_public_complaint(
  p_registration_no text,
  p_student_name text,
  p_complaint_date date,
  p_complaint_type text,
  p_location text,
  p_details text,
  p_assigned_to text
)
returns table(complaint_no bigint)
language plpgsql
security definer
set search_path=''
as $$
begin
  if nullif(trim(p_registration_no),'') is null
     or nullif(trim(p_student_name),'') is null then
    raise exception 'Student details are required';
  end if;

  if p_complaint_type not in ('Plumbing','Electric','Carpentry') then
    raise exception 'Invalid complaint type';
  end if;

  return query
  insert into public.complaints(
    student_id,
    registration_no,
    student_name,
    complaint_date,
    complaint_type,
    location,
    details,
    assigned_to,
    status
  )
  values(
    null,
    trim(p_registration_no),
    trim(p_student_name),
    coalesce(p_complaint_date,current_date),
    p_complaint_type,
    trim(p_location),
    trim(p_details),
    nullif(trim(p_assigned_to),''),
    'PENDING'
  )
  returning public.complaints.complaint_no;
end;
$$;

create or replace function public.track_public_complaints(p_registration_no text)
returns setof public.complaints
language sql
security definer
set search_path=''
as $$
  select c.*
  from public.complaints c
  where lower(trim(c.registration_no))=lower(trim(p_registration_no))
  order by c.complaint_no desc;
$$;

grant execute on function public.submit_public_complaint(text,text,date,text,text,text,text) to anon,authenticated;
grant execute on function public.track_public_complaints(text) to anon,authenticated;

-- Complete a repair atomically: validate stock, deduct stock, record usage,
-- and mark the complaint fixed in one database transaction.
create or replace function public.complete_repair(
  p_complaint_no bigint,
  p_items jsonb
)
returns void
language plpgsql
security definer
set search_path=public
as $$
declare
  v_role text;
  v_item jsonb;
  v_stock integer;
  v_stock_id uuid;
  v_name text;
  v_qty integer;
begin
  select role into v_role
  from public.profiles
  where id=auth.uid();

  if v_role <> 'admin' then
    raise exception 'Only maintenance members can complete repairs';
  end if;

  if not exists (
    select 1
    from public.complaints
    where complaint_no=p_complaint_no
      and status <> 'FIXED'
  ) then
    raise exception 'Repair ID not found or already completed';
  end if;

  for v_item in
    select * from jsonb_array_elements(coalesce(p_items,'[]'::jsonb))
  loop
    v_name=trim(v_item->>'item_name');
    v_qty=(v_item->>'quantity')::integer;

    if v_name is null or v_name='' or v_qty is null or v_qty<=0 then
      raise exception 'Invalid repair item';
    end if;

    select id,quantity into v_stock_id,v_stock
    from public.stock
    where lower(item_name)=lower(v_name)
    for update;

    if v_stock_id is null then
      raise exception 'Stock item not found: %',v_name;
    end if;

    if v_stock<v_qty then
      raise exception 'Insufficient stock for %: only % available',v_name,v_stock;
    end if;
  end loop;

  for v_item in
    select * from jsonb_array_elements(coalesce(p_items,'[]'::jsonb))
  loop
    v_name=trim(v_item->>'item_name');
    v_qty=(v_item->>'quantity')::integer;

    select id into v_stock_id
    from public.stock
    where lower(item_name)=lower(v_name);

    update public.stock
    set quantity=quantity-v_qty,updated_at=now()
    where id=v_stock_id;

    insert into public.inventory_transactions(
      stock_id,item_name,transaction_type,quantity,
      related_complaint_id,performed_by,performed_by_name,
      purpose,notes
    )
    select
      v_stock_id,
      item_name,
      'USED_IN_REPAIR',
      v_qty,
      p_complaint_no,
      auth.uid(),
      p.full_name,
      'Repair completion',
      'Item used for repair ID '||p_complaint_no
    from public.stock s
    left join public.profiles p on p.id=auth.uid()
    where s.id=v_stock_id;
  end loop;

  update public.complaints
  set status='FIXED',
      completed_at=now(),
      completed_by=auth.uid(),
      repair_items=coalesce(p_items,'[]'::jsonb)
  where complaint_no=p_complaint_no;
end;
$$;

grant execute on function public.complete_repair(bigint,jsonb) to authenticated;

-- Generic authenticated/admin inventory functions. These create history as
-- well as changing the current stock quantity.
create or replace function public.record_stock_addition(
  p_item_name text,
  p_quantity integer,
  p_purpose text default null,
  p_notes text default null
)
returns void
language plpgsql
security definer
set search_path=public
as $$
declare
  v_role text;
  v_stock_id uuid;
  v_name text;
  v_user_name text;
begin
  select role,full_name into v_role,v_user_name
  from public.profiles where id=auth.uid();

  if v_role <> 'admin' then
    raise exception 'Only maintenance members can add stock';
  end if;
  if nullif(trim(p_item_name),'') is null or p_quantity is null or p_quantity<=0 then
    raise exception 'Invalid stock entry';
  end if;

  v_name=trim(p_item_name);

  insert into public.stock(item_name,quantity,updated_at)
  values(v_name,p_quantity,now())
  on conflict(item_name) do update
  set quantity=public.stock.quantity+excluded.quantity,
      updated_at=now()
  returning id into v_stock_id;

  insert into public.inventory_transactions(
    stock_id,item_name,transaction_type,quantity,
    performed_by,performed_by_name,purpose,notes
  )
  values(
    v_stock_id,v_name,'STOCK_ADDED',p_quantity,
    auth.uid(),v_user_name,p_purpose,p_notes
  );
end;
$$;

grant execute on function public.record_stock_addition(text,integer,text,text) to authenticated;

-- RLS for the new tables.
alter table public.rooms enable row level security;
alter table public.cupboards enable row level security;
alter table public.stock_locations enable row level security;
alter table public.inventory_transactions enable row level security;
alter table public.item_issues enable row level security;
alter table public.duties enable row level security;
alter table public.srd_members enable row level security;

drop policy if exists "Authenticated users can view rooms" on public.rooms;
create policy "Authenticated users can view rooms"
on public.rooms for select to authenticated using (true);

drop policy if exists "Authenticated users can view cupboards" on public.cupboards;
create policy "Authenticated users can view cupboards"
on public.cupboards for select to authenticated using (true);

drop policy if exists "Authenticated users can view stock locations" on public.stock_locations;
create policy "Authenticated users can view stock locations"
on public.stock_locations for select to authenticated using (true);

drop policy if exists "Admins manage rooms" on public.rooms;
create policy "Admins manage rooms"
on public.rooms for all to authenticated
using ((select role from public.profiles where id=auth.uid())='admin')
with check ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "Admins manage cupboards" on public.cupboards;
create policy "Admins manage cupboards"
on public.cupboards for all to authenticated
using ((select role from public.profiles where id=auth.uid())='admin')
with check ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "Admins manage stock locations" on public.stock_locations;
create policy "Admins manage stock locations"
on public.stock_locations for all to authenticated
using ((select role from public.profiles where id=auth.uid())='admin')
with check ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "Admins view inventory transactions" on public.inventory_transactions;
create policy "Admins view inventory transactions"
on public.inventory_transactions for select to authenticated
using ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "Admins view item issues" on public.item_issues;
create policy "Admins view item issues"
on public.item_issues for select to authenticated
using ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "Admins manage duties" on public.duties;
create policy "Admins manage duties"
on public.duties for all to authenticated
using ((select role from public.profiles where id=auth.uid())='admin')
with check ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "Authenticated users view duties" on public.duties;
create policy "Authenticated users view duties"
on public.duties for select to authenticated using (true);

drop policy if exists "Admins manage srd members" on public.srd_members;
create policy "Admins manage srd members"
on public.srd_members for all to authenticated
using ((select role from public.profiles where id=auth.uid())='admin')
with check ((select role from public.profiles where id=auth.uid())='admin');

drop policy if exists "SRD users view own srd profile" on public.srd_members;
create policy "SRD users view own srd profile"
on public.srd_members for select to authenticated
using (id=auth.uid());
