alter table public.complaints alter column student_id drop not null;

create or replace function public.submit_public_complaint(p_registration_no text,p_student_name text,p_complaint_date date,p_complaint_type text,p_location text,p_details text,p_assigned_to text)
returns table(complaint_no bigint)
language plpgsql security definer set search_path=''
as $$
begin
if nullif(trim(p_registration_no),'') is null or nullif(trim(p_student_name),'') is null then raise exception 'Student details are required'; end if;
if p_complaint_type not in ('Plumbing','Electric','Carpentry') then raise exception 'Invalid complaint type'; end if;
return query insert into public.complaints(student_id,registration_no,student_name,complaint_date,complaint_type,location,details,assigned_to,status)
values(null,trim(p_registration_no),trim(p_student_name),coalesce(p_complaint_date,current_date),p_complaint_type,trim(p_location),trim(p_details),nullif(trim(p_assigned_to),''),'PENDING')
returning public.complaints.complaint_no;
end; $$;

create or replace function public.track_public_complaints(p_registration_no text)
returns setof public.complaints
language sql security definer set search_path=''
as $$
select c.* from public.complaints c where c.registration_no=trim(p_registration_no) order by c.complaint_no desc;
$$;

grant execute on function public.submit_public_complaint(text,text,date,text,text,text,text) to anon,authenticated;
grant execute on function public.track_public_complaints(text) to anon,authenticated;

create or replace function public.complete_repair(
  p_complaint_no bigint,
  p_items jsonb
)
returns void
language plpgsql
security definer
set search_path = public
as $$
declare
  v_role text;
  v_item jsonb;
  v_stock integer;
  v_name text;
  v_qty integer;
begin
  select role into v_role from public.profiles where id = auth.uid();
  if v_role <> 'admin' then
    raise exception 'Only maintenance members can complete repairs';
  end if;

  if not exists (
    select 1 from public.complaints
    where complaint_no = p_complaint_no
      and status <> 'FIXED'
  ) then
    raise exception 'Repair ID not found or already completed';
  end if;

  for v_item in select * from jsonb_array_elements(coalesce(p_items,'[]'::jsonb))
  loop
    v_name := trim(v_item->>'item_name');
    v_qty := (v_item->>'quantity')::integer;

    if v_name is null or v_name = '' or v_qty is null or v_qty <= 0 then
      raise exception 'Invalid repair item';
    end if;

    select quantity into v_stock
    from public.stock
    where lower(item_name) = lower(v_name)
    for update;

    if v_stock is null then
      raise exception 'Stock item not found: %', v_name;
    end if;

    if v_stock < v_qty then
      raise exception 'Insufficient stock for %: only % available', v_name, v_stock;
    end if;
  end loop;

  for v_item in select * from jsonb_array_elements(coalesce(p_items,'[]'::jsonb))
  loop
    v_name := trim(v_item->>'item_name');
    v_qty := (v_item->>'quantity')::integer;

    update public.stock
    set quantity = quantity - v_qty,
        updated_at = now()
    where lower(item_name) = lower(v_name);
  end loop;

  update public.complaints
  set status = 'FIXED',
      completed_at = now(),
      completed_by = auth.uid(),
      repair_items = coalesce(p_items,'[]'::jsonb)
  where complaint_no = p_complaint_no;
end;
$$;

grant execute on function public.complete_repair(bigint,jsonb) to authenticated;
