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