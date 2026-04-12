from subprocess import check_output

Import("env")


def git_hash():
    try:
        return check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=env.subst("$PROJECT_DIR"),
            text=True,
        ).strip()
    except Exception:
        return "unknown"


env.Append(CPPDEFINES=[("T_PAPER_GIT_HASH", '\\"%s\\"' % git_hash())])
