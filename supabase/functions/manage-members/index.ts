import { createClient } from "npm:@supabase/supabase-js@2";

const cors = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
  "Access-Control-Allow-Methods": "POST, OPTIONS"
};

const json = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), {
    status,
    headers: { ...cors, "Content-Type": "application/json" }
  });

function getSecretKey() {
  const raw = Deno.env.get("SUPABASE_SECRET_KEYS");
  if (raw) {
    try {
      const keys = JSON.parse(raw);
      if (keys.default) return keys.default;
      const first = Object.values(keys)[0];
      if (typeof first === "string") return first;
    } catch {}
  }
  const legacy = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
  if (legacy) return legacy;
  throw new Error("Supabase secret key is not configured");
}

function getPublishableKey() {
  const raw = Deno.env.get("SUPABASE_PUBLISHABLE_KEYS");
  if (raw) {
    try {
      const keys = JSON.parse(raw);
      if (keys.default) return keys.default;
      const first = Object.values(keys)[0];
      if (typeof first === "string") return first;
    } catch {}
  }
  const legacy = Deno.env.get("SUPABASE_ANON_KEY");
  if (legacy) return legacy;
  throw new Error("Supabase publishable key is not configured");
}

const url = Deno.env.get("SUPABASE_URL")!;
const admin = createClient(url, getSecretKey(), {
  auth: { autoRefreshToken: false, persistSession: false, detectSessionInUrl: false }
});
const publicClient = createClient(url, getPublishableKey(), {
  auth: { autoRefreshToken: false, persistSession: false, detectSessionInUrl: false }
});

async function requireAdmin(req: Request) {
  const auth = req.headers.get("Authorization") || "";
  if (!auth.startsWith("Bearer ")) throw new Error("Authentication required");
  const token = auth.slice(7);
  const { data, error } = await admin.auth.getUser(token);
  if (error || !data.user) throw new Error("Authentication required");

  const { data: profile, error: pe } = await admin
    .from("profiles")
    .select("id,role,full_name")
    .eq("id", data.user.id)
    .single();

  if (pe || !profile || profile.role !== "admin") throw new Error("Admin access required");
  return profile;
}

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") return new Response("ok", { headers: cors });

  try {
    const body = await req.json();
    const action = body.action;

    if (action === "login") {
      const memberId = String(body.member_id || "").trim();
      const password = String(body.password || "");
      if (!memberId || !password) return json({ error: "Member ID and password are required" }, 400);

      const { data: profile, error: pe } = await admin
        .from("profiles")
        .select("id,registration_no,full_name,role")
        .eq("registration_no", memberId)
        .eq("role", "admin")
        .single();

      if (pe || !profile) return json({ error: "Invalid Login!" }, 401);

      const { data: userResult, error: ue } = await admin.auth.admin.getUserById(profile.id);
      if (ue) throw ue;

      const email = userResult.user?.email || null;
      if (!email) return json({ error: "Invalid Login!" }, 401);

      const { data, error } = await publicClient.auth.signInWithPassword({
        email,
        password
      });

      if (error || !data.session || !data.user) return json({ error: "Invalid Login!" }, 401);

      return json({
        session: {
          access_token: data.session.access_token,
          refresh_token: data.session.refresh_token
        },
        profile: {
          id: profile.id,
          registration_no: profile.registration_no,
          full_name: profile.full_name,
          role: profile.role
        }
      });
    }

    const actor = await requireAdmin(req);

    if (action === "list") {
      const { data: profiles, error } = await admin
        .from("profiles")
        .select("id,registration_no,full_name,role,created_at")
        .eq("role", "admin")
        .order("registration_no");

      if (error) throw error;

      const { data: users, error: ue } = await admin.auth.admin.listUsers({ page: 1, perPage: 1000 });
      if (ue) throw ue;

      const emailById = new Map((users.users || []).map(u => [u.id, u.email || ""]));
      return json({
        members: (profiles || []).map(p => ({
          ...p,
          email: emailById.get(p.id) || ""
        }))
      });
    }

    if (action === "create") {
      const memberId = String(body.member_id || "").trim();
      const name = String(body.name || "").trim();
      const email = String(body.email || "").trim().toLowerCase();
      const password = String(body.password || "");

      if (!memberId || !name || !email || !password) {
        return json({ error: "Member ID, name, email and initial password are required" }, 400);
      }
      if (!/^\S+@\S+\.\S+$/.test(email)) return json({ error: "Enter a valid email address" }, 400);
      if (password.length < 6) return json({ error: "Password must be at least 6 characters" }, 400);

      const { data: existing } = await admin
        .from("profiles")
        .select("id")
        .eq("registration_no", memberId)
        .maybeSingle();

      if (existing) return json({ error: "That Member ID already exists" }, 409);

      const { data: created, error: ce } = await admin.auth.admin.createUser({
        email,
        password,
        email_confirm: true,
        user_metadata: {
          registration_no: memberId,
          full_name: name
        }
      });

      if (ce || !created.user) throw ce || new Error("Could not create account");

      const { error: pe } = await admin
        .from("profiles")
        .update({
          registration_no: memberId,
          full_name: name,
          role: "admin"
        })
        .eq("id", created.user.id);

      if (pe) {
        await admin.auth.admin.deleteUser(created.user.id);
        throw pe;
      }

      return json({
        member: {
          id: created.user.id,
          registration_no: memberId,
          full_name: name,
          email
        }
      }, 201);
    }

    if (action === "deactivate") {
      const memberId = String(body.member_id || "").trim();
      if (!memberId) return json({ error: "Member ID is required" }, 400);
      if (memberId === String(actor.id)) return json({ error: "You cannot deactivate your own account here" }, 400);

      const { data: profile, error: pe } = await admin
        .from("profiles")
        .select("id")
        .eq("registration_no", memberId)
        .eq("role", "admin")
        .single();

      if (pe || !profile) return json({ error: "Member not found" }, 404);

      const { error: ue } = await admin.auth.admin.updateUserById(profile.id, {
        ban_duration: "876000h"
      });
      if (ue) throw ue;

      return json({ ok: true });
    }

    return json({ error: "Unknown action" }, 400);
  } catch (e) {
    return json({ error: e instanceof Error ? e.message : "Server error" }, 500);
  }
});