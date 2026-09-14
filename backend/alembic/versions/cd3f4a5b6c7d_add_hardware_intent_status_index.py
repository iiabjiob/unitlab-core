"""index hardware intent recovery states"""

from alembic import op


revision = "cd3f4a5b6c7d"
down_revision = "cc2e3f4a5b6c"
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.create_index(
        "ix_hardware_command_intents_status_execution",
        "hardware_command_intents",
        ["status", "execution_status"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index(
        "ix_hardware_command_intents_status_execution",
        table_name="hardware_command_intents",
    )
