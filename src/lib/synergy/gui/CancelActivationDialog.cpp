/*
 * Deskflow -- mouse and keyboard sharing utility
 * Copyright (C) 2016 Symless Ltd.
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CancelActivationDialog.h"

#include "ui_CancelActivationDialog.h"

#include "QPushButton"

CancelActivationDialog::CancelActivationDialog(QWidget *parent) : QDialog(parent), ui(new Ui::CancelActivationDialog)
{
  ui->setupUi(this);

  ui->m_pButtonBox->button(QDialogButtonBox::Cancel)->setText("&Back");
  ui->m_pButtonBox->button(QDialogButtonBox::Ok)->setText("&Exit");
}

CancelActivationDialog::~CancelActivationDialog()
{
  delete ui;
}
